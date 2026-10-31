// Copyright 2000-2013 Makoto Mori, Nobuyuki Oba
// Copyright 2026 MadModem contributors
//
// This file is part of MadModem and contains an SSTV RX core derived from
// concepts and constants in MMSSTV's LGPL `sstv.cpp` / `sstv.h`.
//
// MadModem is free software: you can redistribute this file and/or modify it
// under the terms of the GNU Lesser General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// This file is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE.  See COPYING.LESSER.txt and COPYING.txt in this folder.

#ifndef MADMODEM_MMSSTV_RX_CORE_H
#define MADMODEM_MMSSTV_RX_CORE_H

#include <array>
#include <cmath>
#include <cstddef>

namespace mmsstv_lgpl {

/**
 * @brief MMSSTV-style zero-crossing frequency-to-level converter.
 *
 * MMSSTV's CFQC class measures the interval between zero crossings after the
 * receive filter and converts it to a normalized SSTV video level around a
 * 1900 Hz centre and 400 Hz half-bandwidth.  This class is a compact, portable
 * Qt/CMake-friendly port of that idea for MadModem's SSTV RX path.
 */
class FqcToneEstimator
{
public:
    FqcToneEstimator();

    void setSampleRate(double sampleRateHz);
    void reset();

    /**
     * @brief Process one normalized audio sample and return estimated tone Hz.
     */
    double processSample(double sample);

    double lastFrequencyHz() const { return m_lastFrequencyHz; }

private:
    void updateFilterCoefficients();
    double bandLimit(double sample);
    void updateFromCrossing(double filteredSample);

private:
    double m_sampleRateHz = 48000.0;

    // Lightweight input conditioning before the zero-crossing FQC.  The
    // original MMSSTV receive chain has FIR/IIR filters; these one-pole stages
    // are deliberately conservative and portable but keep the same wide SSTV
    // video band philosophy.
    double m_dc = 0.0;
    double m_prevInput = 0.0;
    double m_highPassState = 0.0;
    double m_highPassAlpha = 0.91;
    double m_lowPassState = 0.0;
    double m_lowPassAlpha = 0.25;

    // CFQC-style zero-crossing state.
    double m_previousFiltered = 0.0;
    double m_count = 0.0;
    double m_accurateCrossingCount = 0.0;
    double m_normalizedFrequency = -1900.0 / 400.0;
    double m_smoothedNormalizedFrequency = 0.0;
    double m_smoothAlpha = 0.10;
    bool m_hasPreviousFiltered = false;

    double m_lastFrequencyHz = 1900.0;
};

/**
 * @brief MMSSTV receive timing constants used by the MadModem renderer.
 *
 * Names mirror MMSSTV CSSTVSET fields where practical:
 * - TW  = whole line time.
 * - KS  = colour scan time.
 * - OF  = offset from line/sync origin to first scan.
 * - SG/SB = second/third scan offsets after OF.
 * - OFP = MMSSTV receive phase offset used by the original auto-sync path.
 */
struct ModeTiming
{
    double twMs = 446.446;
    double ksMs = 146.432;
    double ofMs = 5.434;
    double sgMs = 147.004;
    double sbMs = 294.008;
    double ofpMs = 7.2;
    int width = 320;
    int height = 256;
    bool martinOrder = true;
};

ModeTiming timingForMadModemMode(const char *label);

} // namespace mmsstv_lgpl

#endif // MADMODEM_MMSSTV_RX_CORE_H
