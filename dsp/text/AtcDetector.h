#ifndef ATCDETECTOR_H
#define ATCDETECTOR_H

/**
 * @brief Result of one adaptive threshold control FSK decision sample.
 */
struct AtcDecision
{
    double soft = 0.0;          ///< positive = mark, negative = space
    double quality = 0.0;       ///< absolute decision confidence, 0..1-ish
    double totalEnergy = 0.0;   ///< raw mark+space energy
    double snrLike = 1.0;       ///< energy / tracked noise floor
    double balance = 1.0;       ///< selective-fading balance estimate
    bool carrierPresent = false;
};

/**
 * @brief Adaptive threshold control for two-tone FSK/RTTY.
 *
 * The detector normalizes Mark and Space energy independently before slicing.
 * This is the important part for HF fading: a fixed midpoint between the two
 * channels fails when one tone fades more than the other.
 */
class AtcDetector
{
public:
    void reset();
    AtcDecision process(double markEnergy, double spaceEnergy, double inputPower);

private:
    double m_markReference = 1.0e-9;
    double m_spaceReference = 1.0e-9;
    double m_noiseFloor = 1.0e-10;
    double m_presenceScore = 0.0;
};

#endif // ATCDETECTOR_H
