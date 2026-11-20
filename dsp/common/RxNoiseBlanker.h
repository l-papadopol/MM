#pragma once
/*
 * RX noise blanker and MSK144 carrier notch (IC8TEM).
 *
 * Streaming version of the MSHV-IC8TEM noise tools (src/HvDecoderMs/hvmsknoise.h,
 * https://github.com/ic8tem/MSHV-IC8TEM), applied to the receive audio before the
 * decoders, like the noise blanker of a radio:
 *
 *  - NB2: impulse blanker after the approach of the "NB2" noise blanker of WDSP by
 *    Warren Pratt NR0V (GPL v2 or later). Impulses are found on the envelope of the
 *    analytic signal against the median level of 4 ms blocks, then blanked with
 *    advance and hang guard times and raised-cosine slews. Events whose core lasts
 *    more than 3 ms (meteor pings, keyed signals) and events framed by a strong level
 *    (the start or the end of a signal) are never touched.
 *  - LPC: linear prediction blanker, port of alt_noise_blanking() by Michael Wild
 *    DL2FW, UHSDR firmware (GPLv3): the samples of short isolated impulses are
 *    replaced by the weighted forward and backward linear prediction; robust
 *    threshold (median absolute deviation) for noise-like digital signals.
 *  - Carrier notch ("ZAP", MSK144 only), like the ZAP of the old WSJT: narrow lines
 *    steady over the last ~5 s are removed with 10 Hz notches; evenly spaced lines
 *    (a continuous MSK signal) are never notched.
 *
 * The blankers work on fixed chunks with a short look-ahead: the output is the
 * input delayed by latencySamples() (48 ms). Callers that keep a UTC timeline
 * subtract latencyNs() from the capture time of the processed samples. With the
 * blanker off nothing is processed and the delay is zero.
 *
 * Copyright (C) 2026 Costantino Cerrotta, IC8TEM.
 * This program is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 */

#include <QString>
#include <QVector>
#include <QtGlobal>

#include <complex>
#include <vector>

namespace MadModemNoise {

enum class BlankerMode : int { Off = 0, Nb2 = 1, Lpc = 2 };
enum class BlankerLevel : int { Sensitive = 0, Normal = 1, StrongOnly = 2 };

struct Settings
{
    BlankerMode mode = BlankerMode::Off;
    BlankerLevel level = BlankerLevel::Normal;
    bool mskCarrierNotch = false;
};

/** Process-wide settings: written by the GUI thread, read by every decoder thread. */
Settings settings();
void setSettings(const Settings &settings);
/** Changes every time setSettings() changes a value. */
quint64 settingsGeneration();
/** Impulses blanked or repaired since start, all decoders together (status display). */
quint64 totalEvents();

QString modeName(BlankerMode mode);
double nb2Threshold(BlankerLevel level);
double lpcThreshold(BlankerLevel level);

/** Radix-2 complex FFT used by the blanker and the carrier notch. */
class Fft
{
public:
    void resize(int n);
    int size() const { return m_n; }
    void forward(std::complex<double> *data) const { run(data, false); }
    void inverse(std::complex<double> *data) const { run(data, true); }

private:
    void run(std::complex<double> *data, bool inverse) const;
    int m_n = 0;
    std::vector<std::complex<double>> m_twiddles;
    std::vector<int> m_bitReverse;
};

/** NB2 or LPC impulse blanker on a real audio stream, any sample rate. */
class ImpulseBlanker
{
public:
    void configure(BlankerMode mode, double threshold, int sampleRate);
    BlankerMode mode() const { return m_mode; }
    int sampleRate() const { return m_fs; }
    /** Delay of the output, 0 when the blanker is off. */
    int latencySamples() const { return m_mode == BlankerMode::Off ? 0 : m_c + m_l; }
    void reset();
    /** Processes n samples in place; the output is the input delayed by latencySamples(). */
    void process(double *x, int n);
    quint64 events() const { return m_events; }

private:
    void step();
    void detectNb2();
    void repairLpcFrame(int start);

    BlankerMode m_mode = BlankerMode::Off;
    double m_threshold = 5.0;
    int m_fs = 0;
    // Window: [history m_h][chunk m_c][look-ahead m_l]; the chunk is output after each step.
    int m_h = 0, m_c = 0, m_l = 0, m_w = 0;
    // NB2 times in samples.
    int m_block = 0, m_gap = 0, m_coreMax = 0, m_eventMax = 0, m_guard = 0, m_slew = 0, m_side = 0;
    // LPC geometry in samples.
    int m_order = 10, m_frame = 128, m_half = 5;
    std::vector<double> m_buffer, m_gain, m_out;
    std::vector<std::complex<double>> m_analytic;
    std::vector<double> m_env, m_blockMedian, m_reference, m_scratch;
    Fft m_fft;
    std::size_t m_outRead = 0;
    int m_fill = 0;
    qint64 m_windowStart = 0;
    qint64 m_lastEventEnd = -1;
    quint64 m_events = 0;
};

/** Steady narrow carriers notched out of a 12 kHz stream (MSK144 "ZAP"). */
class CarrierNotch
{
public:
    static constexpr int kSampleRate = 12000;
    void reset();
    /** Processes n samples at 12 kHz in place, no delay. */
    void process(double *x, int n);
    int lineCount() const { return static_cast<int>(m_notches.size()); }
    QVector<double> lines() const;

private:
    struct Notch
    {
        double hz = 0.0;
        double b0 = 1.0, b1 = 0.0, b2 = 1.0, a1 = 0.0, a2 = 0.0;
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
    };
    void analyzeSlice();
    void updateNotches(const std::vector<double> &hz);

    std::vector<double> m_slice;
    std::vector<double> m_ring;   // spectra of the last slices, normalized
    std::vector<double> m_tones;  // MSK tone metric of each slice
    int m_bins = 0, m_firstBin = 0, m_slices = 0, m_pos = 0;
    std::vector<Notch> m_notches;
    Fft m_fft;
    std::vector<std::complex<double>> m_work;
};

/**
 * One per decoder: blanker plus optional carrier notch, following the global settings.
 */
class RxNoiseStage
{
public:
    explicit RxNoiseStage(bool carrierNotchAllowed = false) : m_notchAllowed(carrierNotchAllowed) {}
    /**
     * Applies the current global settings for this sample rate. Returns true when the
     * delay of the stage changed; the stage is then reset and the caller must restart
     * its sample timeline.
     */
    bool update(int sampleRate);
    bool active() const { return m_blanker.mode() != BlankerMode::Off || m_notchOn; }
    int latencySamples() const { return m_blanker.latencySamples(); }
    qint64 latencyNs() const;
    void reset();
    void process(QVector<double> &samples);
    void process(QVector<float> &samples);
    void process(double *x, int n);
    quint64 events() const { return m_blanker.events(); }
    int notchedLines() const { return m_notchOn ? m_notch.lineCount() : 0; }

    /**
     * Applies the current global settings to a whole recording, with the delay
     * removed: sample i of the output belongs to sample i of the input.
     */
    static void processRecording(QVector<double> &samples, int sampleRate, bool carrierNotchAllowed = false);
    static void processRecording(QVector<float> &samples, int sampleRate, bool carrierNotchAllowed = false);

private:
    bool m_notchAllowed = false;
    bool m_notchOn = false;
    quint64 m_generation = ~quint64{0};
    int m_fs = 0;
    ImpulseBlanker m_blanker;
    CarrierNotch m_notch;
    std::vector<double> m_temp;
};

} // namespace MadModemNoise
