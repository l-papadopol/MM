// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Papadopol Lucian-Ioan.
#include "CwNeuralDecoder.h"
#include "model/CwNeuralWeights.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace madmodem::cwskimmer {
namespace {
constexpr double pi = 3.14159265358979323846;

struct InferenceKernels {
    std::array<std::vector<float>,4> layers;
    std::array<float,48*42> head{};
    InferenceKernels() {
        using namespace neural_weights;
        const float* original[]={layers_0_weight,layers_1_weight,layers_2_weight,layers_3_weight};
        const float* outputWeights=head_weight;
        for (int layer=0;layer<4;++layer) {
            const int channels=layer==0 ? 9 : 48;
            layers[layer].resize(channels*5*48);
            for (int in=0;in<channels;++in)
                for (int k=0;k<5;++k)
                    for (int out=0;out<48;++out)
                        layers[layer][(in*5+k)*48+out]=original[layer][(out*channels+in)*5+k];
        }
        for (int in=0;in<48;++in)
            for (int out=0;out<42;++out)
                head[in*42+out]=outputWeights[out*48+in];
    }
};

const InferenceKernels& inferenceKernels() {
    // Immutable shared layout: output channels are adjacent for SIMD. Each
    // output still accumulates products in exactly the original in/k order.
    static const InferenceKernels kernels;
    return kernels;
}
}

void CwNeuralDecoder::setToneHz(double hz) {
    hz = std::clamp(hz, 100., 3500.);
    if (std::abs(hz-m_tone) < .5) return;
    m_tone = hz;
    reset();
}

void CwNeuralDecoder::reset() {
    m_features.fill(0.f);
    constexpr int dilations[]={1,4,16,64};
    for (int layer=0;layer<4;++layer)
        m_layerHistory[layer].assign((4*dilations[layer]+1)*(layer==0 ? 9 : 48),0.f);
    m_frameNumber=0;
    m_sum.fill({0.,0.}); m_phase.fill({1.,0.});
    for (auto& h : m_history) std::fill(h.begin(), h.end(), std::complex<double>{});
    std::fill(m_energyHistory.begin(), m_energyHistory.end(), 0.);
    m_peakEnergy = 0.; m_energy = 0.; m_position = m_samples = 0;
    m_haveText = false; m_lastWasSpace = true;
    m_prefix.reset();
}

void CwNeuralDecoder::configure(int rate) {
    m_rate=rate;
    m_window=std::max(1, static_cast<int>(std::lround(rate*.032)));
    m_hop=std::max(1, static_cast<int>(std::lround(rate*.008)));
    for (int j=0;j<9;++j) {
        m_rotation[j] = std::polar(1., -2*pi*(m_tone+(j-4)*62.5)/rate);
        m_history[j].assign(static_cast<std::size_t>(m_window), {});
    }
    m_energyHistory.assign(static_cast<std::size_t>(m_window), 0.);
    reset();
}

std::array<float, 42> CwNeuralDecoder::inferFeatures(const std::array<float,9>& features) {
    using namespace neural_weights;
    static_assert(kChannels==48, "Model/runtime channel dimension mismatch");
    static_assert(sizeof(kAlphabet)-1==41, "Model/runtime alphabet mismatch");
    if (m_layerHistory[0].empty()) reset();
    constexpr int dilations[]={1,4,16,64};
    const auto& kernels=inferenceKernels();
    const float* biases[]={layers_0_bias,layers_1_bias,layers_2_bias,layers_3_bias};
    std::array<float,48> input{}, next{};
    std::copy(features.begin(),features.end(),input.begin());
    for (int layer=0;layer<4;++layer) {
        const int channels=layer==0 ? 9 : 48;
        const std::size_t length=4*dilations[layer]+1;
        const std::size_t position=m_frameNumber%length;
        auto& history=m_layerHistory[layer];
        std::copy_n(input.data(),channels,history.data()+position*channels);
        // History addresses depend only on the layer/frame. Preserve the
        // exact accumulation order while avoiding a modulus per coefficient.
        const float* taps[5];
        for (int k=0;k<5;++k) {
            const std::size_t delay=(4-k)*dilations[layer];
            taps[k]=history.data()+((position+length-delay)%length)*channels;
        }
        std::copy_n(biases[layer],48,next.data());
        const float* weight=kernels.layers[layer].data();
        for (int in=0;in<channels;++in) {
            for (int k=0;k<5;++k,weight+=48) {
                const float sample=taps[k][in];
                for (int row=0;row<48;++row)
                    next[row]+=weight[row]*sample;
            }
        }
        for (float& value:next) value=std::tanh(value);
        input=next;
    }
    ++m_frameNumber;
    std::array<float,42> logits{};
    std::copy_n(head_bias,42,logits.data());
    for (int j=0;j<48;++j)
        for (int row=0;row<42;++row)
            logits[row]+=kernels.head[j*42+row]*input[j];
    return logits;
}

void CwNeuralDecoder::frame() {
    const double energy=std::max(0.0,m_energy/m_window);
    // Retain key-up amplitude: local RMS alone magnified low-level coherent
    // codec/filter tails until they looked like fully keyed marks. The causal
    // peak releases over 0.5 s; its 20% amplitude floor follows input gain.
    constexpr double release=0.9841273200552851; // exp(-0.008/0.5)
    m_peakEnergy=std::max(energy,m_peakEnergy*release);
    constexpr double floor=.04;
    const double rms=std::sqrt(std::max({energy,floor*m_peakEnergy,1e-12}));
    for (int j=0;j<9;++j)
        m_features[j]=static_cast<float>(std::log1p(5*std::abs(m_sum[j])/m_window/rms));
    const auto logits=inferFeatures(m_features);
    for (int label:m_prefix.consume(logits)) emitLabel(label);
}

void CwNeuralDecoder::emitLabel(int selected) {
    const char ch=neural_weights::kAlphabet[selected-1];
    if (ch!=' ' || (m_haveText && !m_lastWasSpace)) {
        if (m_callback) m_callback(std::string(1,ch));
        m_haveText=true; m_lastWasSpace=ch==' ';
    }
}

void CwNeuralDecoder::process(const float* audio, std::size_t count, int rate) {
    if (!audio || !count || rate<8000 || rate>384000) return;
    if (rate!=m_rate || m_rotation[4]==std::complex<double>{}) configure(rate);
    // A changed marker invalidates the coherent bank, even at the same sample rate.
    const auto expected=std::polar(1.,-2*pi*m_tone/rate);
    if (std::abs(expected-m_rotation[4])>1e-12) configure(rate);
    for (std::size_t i=0;i<count;++i) {
        const double sample=std::isfinite(audio[i]) ? audio[i] : 0.;
        const double e=sample*sample;
        m_energy += e-m_energyHistory[m_position]; m_energyHistory[m_position]=e;
        for (int j=0;j<9;++j) {
            const auto v=sample*m_phase[j];
            m_sum[j] += v-m_history[j][m_position];
            m_history[j][m_position]=v;
            m_phase[j] *= m_rotation[j];
        }
        m_position=(m_position+1)%static_cast<std::size_t>(m_window);
        ++m_samples;
        if (m_samples%static_cast<std::size_t>(m_hop)==0) frame();
        if (m_samples%static_cast<std::size_t>(rate)==0)
            for (auto& phase:m_phase) phase /= std::max(std::abs(phase),1e-20);
    }
}
}
