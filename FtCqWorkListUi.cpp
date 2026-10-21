// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "modems/ft8/FtDecodedText.h"
#include "utils/RuntimeI18n.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QCheckBox>
#include <QItemSelectionModel>
#include <QSignalBlocker>
#include <algorithm>
namespace {
constexpr int kFtDecodeRole=Qt::UserRole+430;
constexpr int kFtPresentationOnlyRole=Qt::UserRole+429;
QString uiText(const char *key,const char *fallback){return MadModemI18n::translate(QString::fromUtf8(key),QString::fromUtf8(fallback));}
}
bool MainWindow::addFtDecodeToCqWorkList(QTableWidgetItem *selected, bool cqOnly)
{
    if(!selected || !m_tableFt8Rx || !Ft8Mode::isFamilyMode(ui->cmbMode->currentText()))return false;
    auto *item=m_tableFt8Rx->item(selected->row(),4);if(!item)return false;
    const auto decode=item->data(kFtDecodeRole).value<Ft8RxDecoder::Decode>();
    if(decode.message.isEmpty())return false; // TX/history rows are never queued.
    const auto parsed=FtDecodedText::parse(decode.message,stationCallsign());
    const auto call=(parsed.cq?FtDecodedText::cqersCall(decode.message):parsed.senderCall).trimmed().toUpper();
    if((cqOnly&&!parsed.cq)||call.isEmpty()||isFtCallBlacklisted(call)||FtDecodedText::callMatches(call,stationCallsign()))return false;
    if(m_ftSession.qsoActive&&FtDecodedText::callMatches(call,m_ftSession.dxCall))return false;
    FtCqWorkList::Entry entry;entry.call=call;entry.grid=parsed.grid;entry.decode=decode;entry.cq=parsed.cq;
    entry.mode=ui->cmbMode->currentText().trimmed().toUpper();
    entry.band=m_cmbFt8Band?m_cmbFt8Band->currentText().trimmed().toLower():m_settings.ft8Band;
    entry.confirmed=!item->data(kFtPresentationOnlyRole).toBool();
    return m_ftCqWorkList.add(entry);
}
void MainWindow::addSelectedFtCqsToWorkList()
{
    if(!m_tableFt8Rx)return;
    auto selected=m_tableFt8Rx->selectionModel()->selectedRows();
    std::sort(selected.begin(),selected.end(),[](const auto&a,const auto&b){return a.row()<b.row();});
    int added=0;
    for(const auto &index:selected)if(addFtDecodeToCqWorkList(m_tableFt8Rx->item(index.row(),4),true))++added;
    appendLog(QStringLiteral("FT CQ list: %1 selected CQ target(s) added; %2 total. Adding targets does not start TX.").arg(added).arg(m_ftCqWorkList.entries.size()));
    refreshFtCqWorkListUi();showFtCqWorkList();
}
void MainWindow::showFtCqWorkList()
{
    if(!m_ftCqDialog){
        m_ftCqDialog=new QDialog(this);m_ftCqDialog->setObjectName(QStringLiteral("ftCqWorkListDialog"));
        m_ftCqDialog->setWindowTitle(uiText("ft_cq_list_title","CQ stations to work"));m_ftCqDialog->resize(670,420);
        auto *layout=new QVBoxLayout(m_ftCqDialog);
        auto *help=new QLabel(uiText("ft_cq_list_help","Select CQ rows with Ctrl/Shift and add them to this list. Start enables one QSO at a time. Before each call MM waits for a new CQ; stations working someone else or no longer heard are skipped. Pause takes effect after the current QSO. STOP stops TX and pauses the list. Enable Manage CQ queue to add stations by repeated double-clicks. The numbered table shows their order; duplicates are ignored."),m_ftCqDialog);
        help->setObjectName(QStringLiteral("ftCqHelp"));help->setWordWrap(true);layout->addWidget(help);
        m_ftCqTable=new QTableWidget(0,5,m_ftCqDialog);m_ftCqTable->setObjectName(QStringLiteral("ftCqWorkListTable"));
        m_ftCqTable->setHorizontalHeaderLabels({QStringLiteral("#"),uiText("callsign","Callsign"),uiText("locator","Locator"),uiText("frequency","Frequency"),uiText("status","Status")});
        m_ftCqTable->setEditTriggers(QAbstractItemView::NoEditTriggers);m_ftCqTable->setSelectionBehavior(QAbstractItemView::SelectRows);m_ftCqTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_ftCqTable->verticalHeader()->hide();m_ftCqTable->horizontalHeader()->setStretchLastSection(true);layout->addWidget(m_ftCqTable);
        auto *buttons=new QHBoxLayout;
        m_btnFtCqRun=new QPushButton(m_ftCqDialog);m_btnFtCqRun->setObjectName(QStringLiteral("ftCqWorkListRun"));buttons->addWidget(m_btnFtCqRun);
        auto *remove=new QPushButton(uiText("ft_cq_list_remove","Remove selected"),m_ftCqDialog);remove->setObjectName(QStringLiteral("ftCqRemove"));buttons->addWidget(remove);
        auto *clear=new QPushButton(uiText("ft_cq_list_clear","Clear list"),m_ftCqDialog);clear->setObjectName(QStringLiteral("ftCqClear"));buttons->addWidget(clear);layout->addLayout(buttons);
        connect(m_btnFtCqRun,&QPushButton::clicked,this,[this]{
            if(!m_ftCqWorkList.running&&m_ftCqWorkList.entries.isEmpty())return;
            m_ftCqWorkList.running=!m_ftCqWorkList.running;
            if(m_ftCqWorkList.running&&m_chkFtCqQueue){const QSignalBlocker blocker(m_chkFtCqQueue);m_chkFtCqQueue->setChecked(true);}
            if(m_ftCqWorkList.running){m_ft8OperatorStopLatched=false;m_ftCqWorkList.requireFreshCheck();
                if(m_chkFt8FullAutoQso&&m_chkFt8FullAutoQso->isChecked())m_chkFt8FullAutoQso->setChecked(false);
                if(!m_rxRunning&&!m_txRunning)startFt8RxShell();
                appendLog(QStringLiteral("FT CQ list started: waiting for fresh CQ before calling each selected target."));
            }else appendLog(QStringLiteral("FT CQ list paused; current QSO remains under the normal sequencer."));
            refreshFtCqWorkListUi();processFtCqWorkList();
        });
        connect(remove,&QPushButton::clicked,this,[this]{auto rows=m_ftCqTable->selectionModel()->selectedRows();std::sort(rows.begin(),rows.end(),[](const auto&a,const auto&b){return a.row()>b.row();});for(const auto&i:rows)if(i.row()<m_ftCqWorkList.entries.size())m_ftCqWorkList.entries.removeAt(i.row());refreshFtCqWorkListUi();});
        connect(clear,&QPushButton::clicked,this,[this]{m_ftCqWorkList.clear();refreshFtCqWorkListUi();});
    }
    refreshFtCqWorkListUi();m_ftCqDialog->show();m_ftCqDialog->raise();m_ftCqDialog->activateWindow();
}
void MainWindow::refreshFtCqWorkListUi()
{
    if(m_btnFtCqList)m_btnFtCqList->setText(uiText("ft_cq_list_button","CQ list (%1)").arg(m_ftCqWorkList.entries.size()));
    if(m_btnFtCqRun)m_btnFtCqRun->setText(m_ftCqWorkList.running?uiText("ft_cq_list_pause","Pause after QSO"):uiText("ft_cq_list_start","Start list"));
    if(m_chkFtCqQueue){m_chkFtCqQueue->setText(uiText("ft_cq_queue_enable","Manage CQ queue"));m_chkFtCqQueue->setToolTip(uiText("ft_cq_queue_tooltip","When enabled, double-clicks add stations to the CQ list without interrupting the current QSO. Press Start list to begin."));}
    if(m_btnFtCqAdd)m_btnFtCqAdd->setText(uiText("ft_cq_list_add","Add selected CQs"));
    if(!m_ftCqTable)return;
    m_ftCqTable->setHorizontalHeaderLabels({QStringLiteral("#"),uiText("callsign","Callsign"),uiText("locator","Locator"),uiText("frequency","Frequency"),uiText("status","Status")});
    m_ftCqTable->setColumnWidth(0,30);
    // Preserve selected calls when preceding rows leave the queue.
    const auto selected=m_ftCqTable->selectionModel()->selectedRows();QSet<QString> selectedCalls;
    for(auto i:selected)if(auto *cell=m_ftCqTable->item(i.row(),1))selectedCalls.insert(cell->text());
    const QSignalBlocker blocker(m_ftCqTable);m_ftCqTable->setRowCount(m_ftCqWorkList.entries.size());
    for(int i=0;i<m_ftCqWorkList.entries.size();++i){const auto&e=m_ftCqWorkList.entries[i];
        QString state=e.confirmed?(e.cq?uiText("ft_cq_list_heard","CQ heard"):uiText("ft_cq_list_busy","Working another station")):uiText("ft_cq_list_wait","Waiting for new CQ");
        const QStringList cells{QString::number(i+1),e.call,e.grid,QString::number(e.decode.frequencyHz),state};
        for(int col=0;col<cells.size();++col){auto *item=m_ftCqTable->item(i,col);if(!item){item=new QTableWidgetItem;m_ftCqTable->setItem(i,col,item);}item->setText(cells[col]);item->setSelected(selectedCalls.contains(e.call));}
    }
}
void MainWindow::processFtCqWorkList()
{
    if(!m_ftCqWorkList.running)return;
    if(m_shutdownInProgress || m_runtimeShutdownComplete){m_ftCqWorkList.running=false;return;}
    if(m_ft8OperatorStopLatched||m_offlineAnalysisActive||!Ft8Mode::isFamilyMode(ui->cmbMode->currentText())){m_ftCqWorkList.running=false;refreshFtCqWorkListUi();return;}
    const bool busy=m_ftSession.qsoActive||m_txRunning||m_ftTxWorkerRunning||m_ft8PendingTxArmed||m_hasDeferredFt8TxPlan||!m_pendingFt8TxPlan.message.trimmed().isEmpty()||(m_txAudioEngine&&m_txAudioEngine->isRunning());
    if(busy){m_ftCqWorkList.requireFreshCheck();return;}
    if(m_chkFt8CallerQueue&&m_chkFt8CallerQueue->isChecked()&&!m_ft8CallerQueue.isEmpty()){
        if(activateNextFt8QueuedCaller())return;
    }
    const auto now=QDateTime::currentMSecsSinceEpoch();const int period=Ft8Mode::profileForMode(ui->cmbMode->currentText()).slotMs;
    m_ftCqWorkList.beginCheck(now,period);
    for(int i=0;i<m_ftCqWorkList.entries.size();){
        const auto entry=m_ftCqWorkList.entries[i];
        const auto state=m_ftCqWorkList.availability(entry,now,period);
        if(state==FtCqWorkList::Availability::Wait)break; // Preserve the operator's order.
        m_ftCqWorkList.entries.removeAt(i);
        if(state!=FtCqWorkList::Availability::Ready){appendLog(QStringLiteral("FT CQ list: skipped %1 (%2).").arg(entry.call,state==FtCqWorkList::Availability::Busy?QStringLiteral("directed traffic after CQ"):QStringLiteral("no fresh CQ in one cycle")));continue;}
        if(entry.mode!=ui->cmbMode->currentText().trimmed().toUpper()||entry.band!=(m_cmbFt8Band?m_cmbFt8Band->currentText().trimmed().toLower():m_settings.ft8Band))continue;
        Ft8FullAutoCqCandidate candidate;candidate.valid=true;candidate.call=entry.call;candidate.grid=entry.grid;candidate.decode=entry.decode;candidate.priorityText=uiText("ft_cq_list_priority","Operator CQ list");
        if(startFt8FullAutoQsoFromCandidate(candidate,true)){m_ftCqWorkList.requireFreshCheck();break;}
    }
    // Keep the explicit Start enabled while idle, so further double-clicks
    // can extend the list during the last active QSO. Pause/STOP/clear disable it.
    refreshFtCqWorkListUi();
}
