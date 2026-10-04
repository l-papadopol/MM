#pragma once
#include "../audio/BoundedAudioDispatcher.h"
#include "../audio/AudioContinuity.h"
#include "../dsp/common/DspConditioner.h"
#include "../modems/rtty/RttyAfc.h"
#include "../modems/rtty/RttyDecoder.h"
#include "../modems/rtty/RttyMultiDecoder.h"
#include "../modems/cw/CwDecoder.h"
#include "../modems/weatherfax/WeatherFaxDecoder.h"
#include "../modems/sstv/SstvDecoder.h"
#include "../modems/bpsk31/Bpsk31Decoder.h"
#include "../modems/mfsk/MfskDecoder.h"
#include "../modems/hell/HellschreiberDecoder.h"
#include "../modems/msk144/Msk144Decoder.h"
#include <QHash>
#include <QMutexLocker>
#include <QThread>
#include <tuple>

// One owner for all live non-FT/Q65 decoder state. GUI sends value commands;
// capture feeds a bounded queue directly, without passing through MainWindow.
class RxDecoderWorker : public QObject {
    Q_OBJECT
public:
    struct Graph {
        WeatherFaxDecoder *fax; SstvDecoder *sstv; RttyDecoder *rtty;
        RttyMultiDecoder *multi; Bpsk31Decoder *bpsk; MfskDecoder *mfsk;
        CwDecoder *cw; HellschreiberDecoder *hell; Msk144Decoder *msk;
    };
    struct Config {
        QString mode;
        bool enabled=false, multi=false, afc=false, hellFsk105=false;
        int afcRange=20;
        DspConditioner::Config filter;
    };
    struct CwSnapshot {
        double tone[2]{700,1200}, bandwidth[2]{120,120};
        QString state[2]{"ACQUIRE","ACQUIRE"};
    };
    explicit RxDecoderWorker(Graph graph, QObject *parent=nullptr);
    void configure(const Config &config);
    void beginCapture();
    void drain();
    void updateSnapshots(bool notify=true);
    void acknowledgeImages();
    QImage image(QObject *decoder) const;
    QVector<FrequencyMarker> faxMarkers() const;
    CwSnapshot cwSnapshot() const;
    BoundedAudioDispatcher *queue() { return &m_queue; }
    template<class T, class Method, class... Args>
    void command(T *target, Method method, bool offline, Args... args) {
        auto values=std::make_tuple(std::move(args)...);
        auto operation=[this,target,method,values]() {
            std::apply([&](auto&&... v){(target->*method)(v...);},values);
            updateSnapshots();
        };
        if (QThread::currentThread()==thread()) operation();
        else QMetaObject::invokeMethod(this,operation,offline?Qt::BlockingQueuedConnection:Qt::QueuedConnection);
    }
signals:
    void imagesAvailable();
    void overload(int dropped);
    void afcToneAdjusted(const QString &mode, int toneHz);
private:
    void process(const AudioBlock &block);
    void resetActive();
    Graph m_graph;
    Config m_config;
    BoundedAudioDispatcher m_queue{24};
    AudioContinuity m_continuity;
    DspConditioner m_conditioner;
    RttyAfc::Tracker m_afc;
    int m_afcSamples=0;
    mutable QMutex m_snapshotMutex;
    QHash<QObject*,QImage> m_images;
    bool m_imageNotificationPending=false;
    QVector<FrequencyMarker> m_faxMarkers;
    CwSnapshot m_cwSnapshot;
};
