// Copyright 2000-2013 Makoto Mori, Nobuyuki Oba
// Copyright 2026 MadModem contributors
//
// This file is part of MadModem and contains an SSTV RX core derived from
// concepts and constants in MMSSTV's LGPL `sstv.cpp` / `sstv.h`.
//
// License: GNU Lesser General Public License version 3 or later.

#include "MmsstvRxCore.h"

#include <algorithm>
#include <cstring>

namespace mmsstv_lgpl {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kCenterHz = 1900.0;
constexpr double kHalfBandwidthHz = 400.0;
constexpr double kLowLimitHz = 900.0;
constexpr double kHighLimitHz = 2600.0;

bool startsWith(const char *s, const char *prefix)
{
    if (s == nullptr || prefix == nullptr) {
        return false;
    }

    while (*prefix != '\0') {
        const char a = *s;
        const char b = *prefix;
        if (a == '\0') {
            return false;
        }
        const char la = (a >= 'A' && a <= 'Z') ? static_cast<char>(a - 'A' + 'a') : a;
        const char lb = (b >= 'A' && b <= 'Z') ? static_cast<char>(b - 'A' + 'a') : b;
        if (la != lb) {
            return false;
        }
        ++s;
        ++prefix;
    }
    return true;
}

bool contains(const char *s, const char *needle)
{
    if (s == nullptr || needle == nullptr || *needle == '\0') {
        return false;
    }

    const std::size_t n = std::strlen(needle);
    for (const char *p = s; *p != '\0'; ++p) {
        bool ok = true;
        for (std::size_t i = 0; i < n; ++i) {
            if (p[i] == '\0') {
                ok = false;
                break;
            }
            const char a = p[i];
            const char b = needle[i];
            const char la = (a >= 'A' && a <= 'Z') ? static_cast<char>(a - 'A' + 'a') : a;
            const char lb = (b >= 'A' && b <= 'Z') ? static_cast<char>(b - 'A' + 'a') : b;
            if (la != lb) {
                ok = false;
                break;
            }
        }
        if (ok) {
            return true;
        }
    }
    return false;
}

} // namespace

FqcToneEstimator::FqcToneEstimator()
{
    updateFilterCoefficients();
    reset();
}

void FqcToneEstimator::setSampleRate(double sampleRateHz)
{
    if (sampleRateHz <= 1000.0) {
        sampleRateHz = 48000.0;
    }

    if (std::abs(sampleRateHz - m_sampleRateHz) < 0.5) {
        return;
    }

    m_sampleRateHz = sampleRateHz;
    updateFilterCoefficients();
    reset();
}

void FqcToneEstimator::reset()
{
    m_dc = 0.0;
    m_prevInput = 0.0;
    m_highPassState = 0.0;
    m_lowPassState = 0.0;
    m_previousFiltered = 0.0;
    m_count = 0.0;
    m_accurateCrossingCount = 0.0;
    m_normalizedFrequency = -kCenterHz / kHalfBandwidthHz;
    m_smoothedNormalizedFrequency = 0.0;
    m_hasPreviousFiltered = false;
    m_lastFrequencyHz = kCenterHz;
}

void FqcToneEstimator::updateFilterCoefficients()
{
    const double dt = 1.0 / std::max(1000.0, m_sampleRateHz);

    // Equivalent to a broad MMSSTV receive BPF for 1200 sync plus video tones.
    const double hpCutoffHz = 650.0;
    const double lpCutoffHz = 2800.0;
    const double smoothHz = 900.0;

    const double hpRc = 1.0 / (2.0 * kPi * hpCutoffHz);
    const double lpRc = 1.0 / (2.0 * kPi * lpCutoffHz);

    m_highPassAlpha = hpRc / (hpRc + dt);
    m_lowPassAlpha = dt / (lpRc + dt);
    m_smoothAlpha = 1.0 - std::exp((-2.0 * kPi * smoothHz) / std::max(1000.0, m_sampleRateHz));
    m_smoothAlpha = std::clamp(m_smoothAlpha, 0.015, 0.50);
}

double FqcToneEstimator::bandLimit(double sample)
{
    // Slow DC removal before the explicit HPF keeps the zero crossing stable
    // with USB sound devices that have small bias or LF rumble.
    m_dc += 0.0005 * (sample - m_dc);
    const double centered = sample - m_dc;

    m_highPassState = m_highPassAlpha * (m_highPassState + centered - m_prevInput);
    m_prevInput = centered;

    m_lowPassState += m_lowPassAlpha * (m_highPassState - m_lowPassState);
    return m_lowPassState;
}

void FqcToneEstimator::updateFromCrossing(double filteredSample)
{
    if (!m_hasPreviousFiltered) {
        m_previousFiltered = filteredSample;
        m_hasPreviousFiltered = true;
        return;
    }

    const bool crossedPositive = (filteredSample >= 0.0 && m_previousFiltered < 0.0);
    const bool crossedNegative = (filteredSample < 0.0 && m_previousFiltered >= 0.0);

    if (crossedPositive || crossedNegative) {
        double interval = m_count - m_accurateCrossingCount;
        const double denom = filteredSample - m_previousFiltered;
        double offset = 0.0;

        if (std::abs(denom) > 1.0e-12) {
            offset = filteredSample / denom;
            offset = std::clamp(offset, -1.0, 1.0);
        }

        m_accurateCrossingCount = m_count - offset;
        interval -= offset;

        if (interval >= 1.0) {
            double frequencyHz = m_sampleRateHz * 0.5 / interval;
            frequencyHz = std::clamp(frequencyHz, kLowLimitHz, kHighLimitHz);
            m_normalizedFrequency = (frequencyHz - kCenterHz) / kHalfBandwidthHz;
        }
    }

    m_smoothedNormalizedFrequency +=
        m_smoothAlpha * (m_normalizedFrequency - m_smoothedNormalizedFrequency);

    m_lastFrequencyHz = kCenterHz + (m_smoothedNormalizedFrequency * kHalfBandwidthHz);
    m_lastFrequencyHz = std::clamp(m_lastFrequencyHz, kLowLimitHz, kHighLimitHz);

    m_previousFiltered = filteredSample;
    m_count += 1.0;
}

double FqcToneEstimator::processSample(double sample)
{
    const double filtered = bandLimit(sample);
    updateFromCrossing(filtered);
    return m_lastFrequencyHz;
}

ModeTiming timingForMadModemMode(const char *label)
{
    ModeTiming t;

    // MMSSTV CSSTVSET constants, converted back from samples to ms.
    if (label != nullptr && contains(label, "Martin M2")) {
        t.twMs = 226.798;
        t.ksMs = 73.216;
        t.ofMs = 5.434;
        t.ofpMs = 7.4;
        t.sgMs = 73.788;
        t.sbMs = 147.576;
        t.width = 160;
        t.height = 256;
        t.martinOrder = true;
    } else if (label != nullptr && contains(label, "Martin M3")) {
        t.twMs = 446.446;
        t.ksMs = 146.432;
        t.ofMs = 5.434;
        t.ofpMs = 7.2;
        t.sgMs = 147.004;
        t.sbMs = 294.008;
        t.width = 320;
        t.height = 128;
        t.martinOrder = true;
    } else if (label != nullptr && contains(label, "Martin M4")) {
        t.twMs = 226.798;
        t.ksMs = 73.216;
        t.ofMs = 5.434;
        t.ofpMs = 7.4;
        t.sgMs = 73.788;
        t.sbMs = 147.576;
        t.width = 160;
        t.height = 128;
        t.martinOrder = true;
    } else if (label != nullptr && contains(label, "Scottie DX")) {
        t.twMs = 1050.3;
        t.ksMs = 345.6;
        t.ofMs = 10.5;
        t.ofpMs = 10.2;
        t.sgMs = 347.1;
        t.sbMs = 694.2;
        t.width = 320;
        t.height = 256;
        t.martinOrder = false;
    } else if (label != nullptr && contains(label, "Scottie S2")) {
        t.twMs = 277.692;
        t.ksMs = 88.064;
        t.ofMs = 10.5;
        t.ofpMs = 10.8;
        t.sgMs = 89.564;
        t.sbMs = 179.128;
        t.width = 160;
        t.height = 256;
        t.martinOrder = false;
    } else if (label != nullptr && contains(label, "Scottie S3")) {
        t.twMs = 432.0;
        t.ksMs = 138.24;
        t.ofMs = 10.5;
        t.ofpMs = 10.7;
        t.sgMs = 139.74;
        t.sbMs = 279.48;
        t.width = 320;
        t.height = 128;
        t.martinOrder = false;
    } else if (label != nullptr && contains(label, "Scottie S4")) {
        t.twMs = 277.692;
        t.ksMs = 88.064;
        t.ofMs = 10.5;
        t.ofpMs = 10.8;
        t.sgMs = 89.564;
        t.sbMs = 179.128;
        t.width = 160;
        t.height = 128;
        t.martinOrder = false;
    } else if (label != nullptr && (contains(label, "Scottie") || startsWith(label, "S1"))) {
        t.twMs = 432.0;
        t.ksMs = 138.24;
        t.ofMs = 10.5;
        t.ofpMs = 10.7;
        t.sgMs = 139.74;
        t.sbMs = 279.48;
        t.width = 320;
        t.height = 256;
        t.martinOrder = false;
    } else {
        // Martin 1 default, matching MMSSTV smMRT1.
        t.twMs = 446.446;
        t.ksMs = 146.432;
        t.ofMs = 5.434;
        t.ofpMs = 7.2;
        t.sgMs = 147.004;
        t.sbMs = 294.008;
        t.width = 320;
        t.height = 256;
        t.martinOrder = true;
    }

    return t;
}

} // namespace mmsstv_lgpl
