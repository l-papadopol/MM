#ifndef TEXT_NCO_H
#define TEXT_NCO_H

/**
 * @brief Numerically controlled oscillator for text-mode DSP.
 *
 * CPU-only helper used by RTTY and PSK receivers.  The oscillator keeps phase
 * continuous while allowing small loop corrections from AFC/Costas loops.
 */
class Nco
{
public:
    Nco() = default;

    void configure(double sampleRate, double frequencyHz);
    void reset(double phaseRadians = 0.0);
    void setFrequency(double frequencyHz);
    double frequencyHz() const;
    double phaseRadians() const;

    void mixDown(double sample, double *i, double *q, double extraPhaseStep = 0.0);
    void advance(double extraPhaseStep = 0.0);

private:
    void rebuildIncrement();

private:
    double m_sampleRate = 8000.0;
    double m_frequencyHz = 1000.0;
    double m_phase = 0.0;
    double m_increment = 0.0;
};

#endif // TEXT_NCO_H
