#pragma once
#include <QObject>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <functional>
#include <memory>

class CatCommandJob : public QObject {
    Q_OBJECT
public:
    std::shared_ptr<std::atomic_bool> cancelled;
    std::function<bool()> operation;
    std::function<bool()> rollback;
    void run() {
        bool ok=false;
        if(!cancelled->load())ok=operation();
        bool recovered=true;
        if(!ok || cancelled->load()) { recovered=rollback();ok=false; }
        emit done(ok,recovered,!ok);
    }
public:
    void compensate() {emit done(false,rollback(),true);}
signals:
    void done(bool ok,bool recovered,bool compensated);
};

// GUI-owned gate. No nested event loop and no GUI -> CAT synchronous wait.
// Timeout revokes authorization; the serial CAT worker compensates any late
// hardware completion before accepting a new transaction.
class AsyncCatCommand : public QObject {
    Q_OBJECT
public:
    explicit AsyncCatCommand(QObject *parent=nullptr):QObject(parent){}
    bool busy() const {return m_busy;}
    bool faulted() const {return m_faulted;}
    // A new TX plan or STOP may revoke key-up, never the mandatory key-down.
    void cancel() {if(m_cancelled && !m_recoveryOnly)m_cancelled->store(true);}
    void request(QObject *target,std::function<bool()> operation,
                 std::function<bool()> rollback,std::function<void(bool)> completion,int timeoutMs=3000,bool recoveryOnly=false) {
        if(m_busy || (m_faulted && !recoveryOnly) || !target || !target->thread()->isRunning()){completion(false);return;}
        m_busy=true;
        m_recoveryOnly=recoveryOnly;
        m_cancelled=std::make_shared<std::atomic_bool>(false);
        auto delivered=std::make_shared<bool>(false);
        auto *job=new CatCommandJob;
        job->cancelled=m_cancelled;job->operation=std::move(operation);job->rollback=std::move(rollback);
        job->moveToThread(target->thread());
        connect(target->thread(),&QThread::finished,job,&QObject::deleteLater);
        auto *timer=new QTimer(this);timer->setSingleShot(true);
        connect(timer,&QTimer::timeout,this,[this,delivered,completion](){
            m_cancelled->store(true);
            if(!*delivered){*delivered=true;completion(false);}
        });
        connect(job,&CatCommandJob::done,this,[this,job,timer,delivered,completion,recoveryOnly](bool ok,bool recovered,bool compensated){
            if(ok && m_cancelled->load()){
                QMetaObject::invokeMethod(job,&CatCommandJob::compensate,Qt::QueuedConnection);
                return;
            }
            job->deleteLater();
            timer->stop();timer->deleteLater();m_busy=false;
            m_faulted=!recovered;
            const bool accepted=recoveryOnly ? (ok || recovered) : (ok && !m_cancelled->load());
            m_recoveryOnly=false;
            m_cancelled.reset();
            // Publish confirmed safety recovery even after a timeout was delivered.
            if(compensated)emit compensatedCommand(recovered);
            if(!*delivered){*delivered=true;completion(accepted);}
            if(!m_busy)emit settled();
        });
        timer->start(timeoutMs);
        QMetaObject::invokeMethod(job,&CatCommandJob::run,Qt::QueuedConnection);
    }
    ~AsyncCatCommand() override {cancel();}
signals:
    void compensatedCommand(bool recovered);
    void settled();
private:
    bool m_busy=false, m_faulted=false, m_recoveryOnly=false;
    std::shared_ptr<std::atomic_bool> m_cancelled;
};
