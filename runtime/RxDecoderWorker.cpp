#include "RxDecoderWorker.h"

RxDecoderWorker::RxDecoderWorker(Graph graph,QObject *parent):QObject(parent),m_graph(graph) {
    m_queue.setParent(this);
    for (QObject *decoder : QList<QObject*>{graph.fax,graph.sstv,graph.rtty,graph.multi,graph.bpsk,graph.mfsk,graph.cw,graph.hell,graph.msk}) {
        decoder->setParent(this);
    }
    connect(&m_queue,&BoundedAudioDispatcher::blocksAvailable,this,&RxDecoderWorker::drain,Qt::QueuedConnection);
    connect(graph.rtty,&RttyDecoder::reversePolarityRequested,this,[this](bool reverse){
        m_graph.rtty->setReverse(reverse);
        m_graph.multi->setReverse(reverse);
    },Qt::QueuedConnection);
    updateSnapshots(false);
}
void RxDecoderWorker::configure(const Config &config) {
    Q_ASSERT(QThread::currentThread()==thread());
    if (m_config.mode!=config.mode || m_config.enabled!=config.enabled) {
        m_queue.clear(); m_continuity.reset(); m_conditioner.reset(); m_afc.reset();
    }
    if (m_config.mode!=config.mode) { m_config=config; resetActive(); }
    else m_config=config;
    m_conditioner.setConfig(config.filter);
}
void RxDecoderWorker::beginCapture() {
    m_queue.clear(); m_continuity.reset(); m_conditioner.reset(); m_afc.reset();
}
void RxDecoderWorker::drain() {
    Q_ASSERT(QThread::currentThread()==thread());
    int dropped=0;
    const auto blocks=m_queue.takePending(2,&dropped);
    if (dropped) { resetActive(); m_continuity.reset(); emit overload(dropped); }
    if (!m_config.enabled) return;
    for (const auto &block:blocks) process(block);
}
void RxDecoderWorker::resetActive() {
    m_conditioner.reset(); m_afc.reset(); m_afcSamples=0;
    const auto &mode=m_config.mode;
    if(mode==RttyDecoder::modeName()){m_graph.rtty->reset();m_graph.multi->reset();}
    else if(mode==CwDecoder::modeName())m_graph.cw->reset();
    else if(mode==WeatherFaxDecoder::modeName())m_graph.fax->reset();
    else if(mode==SstvDecoder::modeName())m_graph.sstv->reset();
    else if(mode==Bpsk31Decoder::modeName())m_graph.bpsk->reset();
    else if(mode==MfskDecoder::modeName())m_graph.mfsk->reset();
    else if(mode==HellschreiberDecoder::modeName())m_graph.hell->reset();
    else if(mode==Msk144Decoder::modeName())m_graph.msk->reset();
}
void RxDecoderWorker::process(const AudioBlock &block) {
    if(m_continuity.accept(block))resetActive();
    const auto mode=m_config.mode;
    if(mode==RttyDecoder::modeName() && m_config.afc) {
        const int mark=qRound(m_config.filter.blackHz),shift=qRound(m_config.filter.whiteHz-m_config.filter.blackHz);
        if(m_afc.process(block,mark,shift,m_config.afcRange))m_graph.rtty->retuneTones(mark+m_afc.offsetHz(),mark+shift+m_afc.offsetHz());
    }
    // Preserve the narrow single-tone AFC used by PSK/Hell; DSP is worker-only.
    if(m_config.afc && (mode==Bpsk31Decoder::modeName() || mode==HellschreiberDecoder::modeName())) {
        m_afcSamples+=block.samples.size();
        if(m_afcSamples>=block.sampleRate/4) {
            m_afcSamples=0;
            const int tone=qRound((m_config.filter.blackHz+m_config.filter.whiteHz)/2);
            auto peak=RttyAfc::estimateAfcTonePeak(block,tone,m_config.afcRange,1);
            if(mode==HellschreiberDecoder::modeName() && m_config.hellFsk105){
                const double half=HellschreiberDecoder::fsk105ShiftHz()*0.5;
                const auto low=RttyAfc::estimateAfcTonePeak(block,tone-half,m_config.afcRange,1);
                const auto high=RttyAfc::estimateAfcTonePeak(block,tone+half,m_config.afcRange,1);
                peak.valid=low.valid || high.valid;
                peak.frequencyHz=low.valid && high.valid ? (low.frequencyHz+high.frequencyHz)*0.5
                    : low.valid ? low.frequencyHz+half : high.frequencyHz-half;
            }
            if(peak.valid) {
                const int next=RttyAfc::nudgedToneValue(tone,peak.frequencyHz,3,300,3500);
                if(next!=tone){
                    if(mode==Bpsk31Decoder::modeName())m_graph.bpsk->setToneHz(next);
                    else m_graph.hell->setToneHz(next);
                    const double delta=next-tone;
                    m_config.filter.blackHz+=delta;m_config.filter.whiteHz+=delta;
                    m_conditioner.setConfig(m_config.filter);
                    emit afcToneAdjusted(mode,next);
                }
            }
        }
    }
    if(mode==CwDecoder::modeName())m_graph.cw->processAudioBlock(block);
    else if(mode==Msk144Decoder::modeName())m_graph.msk->processAudioBlock(block);
    else {
        const auto filtered=m_conditioner.processBlock(block);
        if(mode==RttyDecoder::modeName()) {
            m_graph.rtty->processAudioBlock(filtered);
            if(m_config.multi)m_graph.multi->processAudioBlock(block);
        } else if(mode==WeatherFaxDecoder::modeName())m_graph.fax->processAudioBlock(filtered);
        else if(mode==SstvDecoder::modeName())m_graph.sstv->processAudioBlock(filtered);
        else if(mode==Bpsk31Decoder::modeName())m_graph.bpsk->processAudioBlock(filtered);
        else if(mode==MfskDecoder::modeName())m_graph.mfsk->processAudioBlock(filtered);
        else if(mode==HellschreiberDecoder::modeName())m_graph.hell->processAudioBlock(filtered);
    }
    updateSnapshots();
}
void RxDecoderWorker::updateSnapshots(bool notify) {
    // Copy-on-write images and small telemetry only; never hold this mutex in DSP.
    QImage fax=m_graph.fax->currentImage(),sstv=m_graph.sstv->currentImage(),hell=m_graph.hell->currentImage();
    auto markers=m_graph.fax->currentFrequencyMarkers();
    CwSnapshot cw;
    for(int i=0;i<2;++i){cw.tone[i]=m_graph.cw->trackedToneHz(i);cw.bandwidth[i]=m_graph.cw->effectiveBandwidthHz(i);cw.state[i]=m_graph.cw->trackingState(i);}
    bool signal=false;
    {
        QMutexLocker lock(&m_snapshotMutex);
        const bool changed=m_images.value(m_graph.fax).cacheKey()!=fax.cacheKey()
            || m_images.value(m_graph.sstv).cacheKey()!=sstv.cacheKey()
            || m_images.value(m_graph.hell).cacheKey()!=hell.cacheKey();
        m_images[m_graph.fax]=fax;m_images[m_graph.sstv]=sstv;m_images[m_graph.hell]=hell;
        m_faxMarkers=markers;m_cwSnapshot=cw;
        if(notify && changed && !m_imageNotificationPending){m_imageNotificationPending=true;signal=true;}
    }
    if(signal)emit imagesAvailable();
}
void RxDecoderWorker::acknowledgeImages() {QMutexLocker lock(&m_snapshotMutex);m_imageNotificationPending=false;}

QImage RxDecoderWorker::image(QObject *decoder) const {QMutexLocker lock(&m_snapshotMutex);return m_images.value(decoder);}
QVector<FrequencyMarker> RxDecoderWorker::faxMarkers() const {QMutexLocker lock(&m_snapshotMutex);return m_faxMarkers;}
RxDecoderWorker::CwSnapshot RxDecoderWorker::cwSnapshot() const {QMutexLocker lock(&m_snapshotMutex);return m_cwSnapshot;}
