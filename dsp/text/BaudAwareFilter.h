#ifndef BAUDAWAREFILTER_H
#define BAUDAWAREFILTER_H

/**
 * @brief Cascaded one-pole low-pass whose cutoff follows symbol rate.
 *
 * This is a lightweight Nyquist-ish filter for narrow text modes.  It is not a
 * fixed magic bandwidth: each modem configures it from baud/symbol rate and a
 * roll-off factor.  The implementation is deliberately CPU-side and allocation
 * free for real-time audio callbacks.
 */
class BaudAwareLowPass
{
public:
    void configure(double sampleRate,
                   double symbolRate,
                   double rolloff,
                   double minCutoffHz,
                   double maxCutoffHz,
                   int stages = 3);
    void reset();
    double process(double x);

    double cutoffHz() const;
    double alpha() const;

private:
    double m_alpha = 0.01;
    double m_cutoffHz = 40.0;
    int m_stages = 3;
    double m_z1 = 0.0;
    double m_z2 = 0.0;
    double m_z3 = 0.0;
    double m_z4 = 0.0;
};

/**
 * @brief Complex I/Q version of BaudAwareLowPass.
 */
class BaudAwareComplexLowPass
{
public:
    void configure(double sampleRate,
                   double symbolRate,
                   double rolloff,
                   double minCutoffHz,
                   double maxCutoffHz,
                   int stages = 3);
    void reset();
    void process(double iIn, double qIn, double *iOut, double *qOut);
    double cutoffHz() const;

private:
    BaudAwareLowPass m_i;
    BaudAwareLowPass m_q;
};

#endif // BAUDAWAREFILTER_H
