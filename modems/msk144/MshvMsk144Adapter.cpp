#include "MshvMsk144Adapter.h"

#include "../../third_party/mshv_gpl/port/HvDecoderMsMsk/decoderms.h"

#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <utility>
#include <memory>

MshvMsk144Adapter::MshvMsk144Adapter(QObject *parent)
    : QObject(parent)
{
}

MshvMsk144Adapter::~MshvMsk144Adapter() = default;

Msk144RxCoreResult MshvMsk144Adapter::decodePeriod(const QVector<float> &samples12k,
                                                   const QDateTime &periodStartUtc,
                                                   const Msk144RxCoreConfig &config)
{
    Msk144RxCoreResult result;
    m_decodes.clear();
    m_periodStartUtc = periodStartUtc;
    m_rxFrequencyHz = config.rxFrequencyHz;

    if (samples12k.isEmpty()) {
        result.status = QStringLiteral("MSK144 MSHV RX: no samples");
        return result;
    }

    QVector<double> data(samples12k.size());
    for (int i = 0; i < samples12k.size(); ++i) data[i] = static_cast<double>(samples12k.at(i));

    // DecoderMs contains large MSHV work buffers (FFT/correlation/history arrays).
    // Keeping it on the stack can exceed the default Linux GUI-thread stack and
    // terminate offline MSK144 decoding without a Qt error message. Allocate it on
    // the heap so the adapter is safe both for offline tests and live RX.
    std::unique_ptr<DecoderMs> decoder(new DecoderMs(QString()));
    QObject::connect(decoder.get(), SIGNAL(EmitDecodetText(QStringList,bool,bool)),
                     this, SLOT(handleMshvDecode(QStringList,bool,bool)));
    QObject::connect(decoder.get(), SIGNAL(EmitDecodetTextRxFreq(QStringList,bool,bool)),
                     this, SLOT(handleMshvDecode(QStringList,bool,bool)));

    decoder->setMode(0);
    decoder->SetDecoderDeep(qBound(1, config.decodeDepth, 3));
    decoder->SetPerodTime(config.periodSeconds);
    decoder->SetDfSdb(0, qBound(10, config.dfToleranceHz, 500));
    decoder->SetShOpt(config.shortMessages);
    decoder->SetSwlOpt(config.swl);
    // Keep upstream MSHV default: RX equalisation off. Enabling it before
    // the MSHV state is trained can suppress otherwise valid decodes.
    decoder->SetMsk144RxEqual(0);

    QStringList hashContext;
    hashContext << config.myCall.trimmed().toUpper()
                << config.dxCall.trimmed().toUpper()
                << QString() << QString()
                << config.myCall.trimmed().toUpper()
                << config.dxCall.trimmed().toUpper();
    decoder->SetCalsHash(hashContext);

    const QString timeText = periodStartUtc.toUTC().time().toString(QStringLiteral("HH:mm:ss"));
    decoder->DecodeMsk144Buffer(data.data(), data.size(), timeText, 0, false);

    result.decodes = m_decodes;
    result.candidatesTried = 0;
    result.syncCandidates = m_decodes.size();
    result.coarseCandidates = 0;
    result.shortCandidatesTried = 0;
    result.status = QStringLiteral("MSK144 MSHV RX: %1 s, %2 message(s), upstream sync/demod/LDPC path")
                        .arg(samples12k.size() / 12000)
                        .arg(result.decodes.size());
    return result;
}

void MshvMsk144Adapter::handleMshvDecode(QStringList fields, bool, bool)
{
    // MSHV MSK list layout:
    // 0 UTC, 1 T, 2 width, 3 SNR, 4 rpt/blank, 5 DF, 6 message,
    // 7 navg, 8 corrected, 9 eye, 10 ident/freq.
    if (fields.size() < 7) return;
    const QString msg = fields.value(6).trimmed();
    if (msg.isEmpty()) return;

    bool okT = false, okSnr = false, okDf = false;
    const double t = fields.value(1).toDouble(&okT);
    const int snr = fields.value(3).toInt(&okSnr);
    const int df = fields.value(5).toInt(&okDf);

    Msk144CoreDecode d;
    d.utc = m_periodStartUtc;
    d.tSeconds = okT ? t : 0.0;
    d.snrDb = okSnr ? snr : 0;
    d.dfHz = okDf ? df : 0;
    d.frequencyHz = static_cast<double>(m_rxFrequencyHz + d.dfHz);
    d.message = msg;
    d.navg = fields.value(7).toInt();
    d.eye = fields.value(9).toDouble();
    d.shortMessage = msg.startsWith(QLatin1Char('<')) || fields.value(10).contains(QStringLiteral("&"));

    const QString key = QStringLiteral("%1|%2|%3").arg(d.message).arg(d.dfHz).arg(d.tSeconds, 0, 'f', 1);
    for (const Msk144CoreDecode &existing : std::as_const(m_decodes)) {
        const QString existingKey = QStringLiteral("%1|%2|%3").arg(existing.message).arg(existing.dfHz).arg(existing.tSeconds, 0, 'f', 1);
        if (existingKey == key) return;
    }
    m_decodes.append(d);
}
