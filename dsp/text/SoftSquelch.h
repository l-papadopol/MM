#ifndef SOFTSQUELCH_H
#define SOFTSQUELCH_H

/**
 * @brief Hysteretic soft squelch/lock helper.
 */
class SoftSquelch
{
public:
    void configure(double openThreshold, double closeThreshold, double attack, double release);
    void reset();
    bool update(bool candidate);
    bool isOpen() const;
    double score() const;
    void forceClosed();

private:
    double m_openThreshold = 0.60;
    double m_closeThreshold = 0.25;
    double m_attack = 0.05;
    double m_release = 0.015;
    double m_score = 0.0;
    bool m_open = false;
};

#endif // SOFTSQUELCH_H
