/* -*- c++ -*- */
/*
 * Copyright 2018 Florian Lotze
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include <signalprocessing/modulation/ask.h>
#include <cmath>

namespace SignalProcessing {
namespace Modulation {

Ask::Ask() : Ask(1) {}

Ask::Ask(unsigned bitsPerSymbol, bool normalizeOutput) : Modem(), mBpsk(new Bpsk())
{
    /* mBitsPerSymbol excluded from initializer list, because the power
     * normalizer needs to be calculated.
     */
    setBitsPerSymbol(bitsPerSymbol, normalizeOutput);
}

Ask::~Ask() { delete mBpsk; }

void Ask::setBitsPerSymbol(unsigned bps, bool normalizeOutput)
{
    mBitsPerSymbol = bps;

    if (normalizeOutput) {
        // Calculate the normalization factor, based on average constellation power.
        // This requires input data distribution to be uniform.
        float power = 0.0;
        float limit = 1 << mBitsPerSymbol;
        for (float symbol = 1.0; symbol < limit; symbol += 2.0) {
            power += symbol * symbol;
        }
        mNormalMagnitude = sqrt(2.0 * power / limit);
        mPowerNormalizer = 1.0 / mNormalMagnitude;
    } else {
        mNormalMagnitude = 1.0;
        mPowerNormalizer = 1.0;
    }
}

unsigned Ask::bitsPerSymbol() { return mBitsPerSymbol; }

void Ask::modulate()
{
    /* BPSK premodulation and ASK can be merged to improve performance */
    mBpsk->setInputSignal(mInputSignal);
    mBpsk->modulate();

    std::vector<float>* bpskSignal = mBpsk->outputSignal();

    unsigned padding = bpskSignal->size() % mBitsPerSymbol;
    if (padding != 0) {
        while (padding < mBitsPerSymbol) {
            bpskSignal->push_back(1.0f);
            padding++;
        }
    }

    const size_t symbolCount = bpskSignal->size() / mBitsPerSymbol;
    mOutputSignal->resize(symbolCount);

    const float* fiData = bpskSignal->data();
    float* foData = mOutputSignal->data();

    unsigned inputBitCounter = 0;
    for (size_t i = 0; i < symbolCount; ++i) {
        float symbol = 0.0;
        float memory = 1.0;
        for (unsigned bit = 0; bit < mBitsPerSymbol; ++bit) {
            memory *= fiData[inputBitCounter++];
            symbol = 2 * symbol + memory;
        }
        foData[i] = symbol * mPowerNormalizer;
    }
}

void Ask::demodulate()
{
    const size_t symbolCount = mInputSignal->size();
    const size_t bitCount = symbolCount * mBitsPerSymbol;
    mOutputSignal->resize(bitCount);

    unsigned outputBitCounter = 0;
    float* foData = mOutputSignal->data();
    float* fiData = mInputSignal->data();
    for (size_t symbol = 0; symbol < symbolCount; ++symbol) {
        float amplitude = fiData[symbol] * mNormalMagnitude;
        float shift = 1 << (mBitsPerSymbol - 1);
        for (unsigned bit = 0; bit < mBitsPerSymbol; ++bit) {
            foData[outputBitCounter++] = amplitude;
            amplitude = fabs(amplitude) - shift;
            shift /= 2;
        }
    }
}

} // namespace Modulation
} // namespace SignalProcessing
