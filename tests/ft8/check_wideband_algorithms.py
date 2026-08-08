#!/usr/bin/env python3
"""Static and numeric audit for the FT8/FT4 wideband sensitivity patch.

This does not replace a Qt build or a same-WAV decode comparison.  It checks
source invariants and the index/mapping mathematics copied from the directly
inspected WSJT-X/MSHV paths.
"""
from __future__ import annotations

import hashlib
import math
import random
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CPP = (ROOT / "modems/ft8/Ft8RxDecoder.cpp").read_text(encoding="utf-8")
HDR = (ROOT / "modems/ft8/Ft8RxDecoder.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def extract_ghost_block(text: str) -> str:
    start = text.index("    /*\n     * v4.13 LDPC load-shed gate.")
    end_marker = "    if (ldpcGhostCandidate) {\n        return reject(DecodeRejectReason::SoftMetric);\n    }\n\n"
    end = text.index(end_marker, start) + len(end_marker)
    return text[start:end]


def extract_cpp_function(text: str, signature: str) -> str:
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise AssertionError(f"unterminated function: {signature}")


def solve5(matrix: list[list[float]]) -> list[float]:
    a = [row[:] for row in matrix]
    for col in range(5):
        pivot = max(range(col, 5), key=lambda row: abs(a[row][col]))
        require(abs(a[pivot][col]) > 1e-12, "singular polynomial test matrix")
        a[col], a[pivot] = a[pivot], a[col]
        inv = 1.0 / a[col][col]
        for j in range(col, 6):
            a[col][j] *= inv
        for row in range(5):
            if row == col:
                continue
            factor = a[row][col]
            for j in range(col, 6):
                a[row][j] -= factor * a[col][j]
    return [a[i][5] for i in range(5)]


def fit4(points: list[tuple[float, float]]) -> list[float]:
    a = [[0.0] * 6 for _ in range(5)]
    for x, y in points:
        xp = [1.0]
        for _ in range(8):
            xp.append(xp[-1] * x)
        for row in range(5):
            for col in range(5):
                a[row][col] += xp[row + col]
            a[row][5] += y * xp[row]
    return solve5(a)


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    index = round(max(0.0, min(1.0, fraction)) * (len(ordered) - 1))
    return ordered[index]


def test_baseline_fit() -> None:
    rng = random.Random(0xF84)
    n = 500
    midpoint = (n - 1) / 2.0
    scale = n / 2.0
    truth: list[float] = []
    observed: list[float] = []
    for i in range(n):
        x = (i - midpoint) / scale
        base = -72.0 + 1.8 * x + 1.1 * x * x - 0.45 * x**3 + 0.22 * x**4
        # Power spectra sit above the lower envelope.  Keep some bins close to
        # it and add deterministic strong signals/interference.
        excess = abs(rng.gauss(0.0, 1.0)) * 1.6
        if i in (41, 99, 187, 188, 310, 421):
            excess += 18.0
        truth.append(base + 0.65)
        observed.append(base + excess)

    points: list[tuple[float, float]] = []
    segments = 10
    for segment in range(segments):
        a = n * segment // segments
        b = n * (segment + 1) // segments
        cut = percentile(observed[a:b], 0.10)
        for i in range(a, b):
            if observed[i] <= cut:
                points.append(((i - midpoint) / scale, observed[i]))
    coeff = fit4(points)
    fitted = []
    for i in range(n):
        x = (i - midpoint) / scale
        fitted.append(coeff[0] + x * (coeff[1] + x * (coeff[2] + x * (coeff[3] + x * coeff[4]))) + 0.65)
    rmse = math.sqrt(sum((a - b) ** 2 for a, b in zip(fitted, truth)) / n)
    require(rmse < 1.2, f"robust baseline synthetic RMSE too high: {rmse:.3f} dB")


def test_ft4_tone_mapping() -> None:
    match = re.search(r"constexpr int kFt4GrayMap\[4\] = \{([^}]+)\};", CPP)
    require(match is not None, "FT4 Gray map missing")
    gray = [int(x.strip()) for x in match.group(1).split(",")]
    require(gray == [0, 1, 3, 2], f"unexpected FT4 Gray map: {gray}")
    rng = random.Random(0x17491)
    for _ in range(1000):
        b0 = rng.randrange(2)
        b1 = rng.randrange(2)
        idx_cpp = (b0 << 1) | b1
        idx_mshv = b1 + 2 * b0
        require(idx_cpp == idx_mshv, "FT4 bit-pair index differs from MSHV")
        require(gray[idx_cpp] in range(4), "FT4 mapped tone out of range")


def test_ft4_global_metric_indices() -> None:
    # WSJT-X extracts 58 bits after each 8-bit Costas block.
    extracted = list(range(8, 66)) + list(range(74, 132)) + list(range(140, 198))
    require(len(extracted) == 174 and len(set(extracted)) == 174,
            "FT4 extracted bit ranges are not 174 unique data bits")
    sync = set(range(0, 8)) | set(range(66, 74)) | set(range(132, 140)) | set(range(198, 206))
    require(not (set(extracted) & sync), "FT4 data extraction includes sync bits")
    # A four-symbol group over 103 symbols ends at symbol 99; bits 200..205
    # must be filled from the 2- and 1-symbol families.
    starts4 = list(range(0, 103 - 4 + 1, 4))
    require(starts4[-1] == 96, "FT4 four-symbol tail assumption changed")
    starts2 = list(range(0, 103 - 2 + 1, 2))
    require(starts2[-1] == 100, "FT4 two-symbol tail assumption changed")


def test_ft8_group_crossing() -> None:
    for symbol_base in (7, 43):
        # 2-symbol last group starts at local data index 28 and reaches sync.
        require(symbol_base + 28 + 1 in (36, 72), "FT8 2-symbol tail no longer reaches Costas")
        # 3-symbol last group starts at local data index 27 and reaches sync.
        require(symbol_base + 27 + 2 in (36, 72), "FT8 3-symbol tail no longer reaches Costas")
        require(symbol_base + 27 + 2 < 79, "FT8 coherent group exceeds symbol array")



def grouped_metrics(symbols: list[list[complex]], bits_per_symbol: int, group_size: int) -> list[float]:
    total_bits = len(symbols) * bits_per_symbol
    metric = [0.0] * total_bits
    combo_count = 1 << (bits_per_symbol * group_size)
    for symbol_start in range(0, len(symbols) - group_size + 1, group_size):
        scores = [0.0] * combo_count
        for combo in range(combo_count):
            coherent = 0j
            for g in range(group_size):
                shift = bits_per_symbol * (group_size - 1 - g)
                index = (combo >> shift) & ((1 << bits_per_symbol) - 1)
                coherent += symbols[symbol_start + g][index]
            scores[combo] = abs(coherent)
        for bit in range(bits_per_symbol * group_size):
            global_bit = symbol_start * bits_per_symbol + bit
            if global_bit >= total_bits:
                continue
            mask = 1 << (bits_per_symbol * group_size - 1 - bit)
            best0 = max(score for combo, score in enumerate(scores) if not (combo & mask))
            best1 = max(score for combo, score in enumerate(scores) if combo & mask)
            metric[global_bit] = best0 - best1
    return metric


def test_ft4_coherent_metric_sign_and_ranges() -> None:
    rng = random.Random(0xF740)
    data_bits = [rng.randrange(2) for _ in range(174)]
    sync_ranges = set(range(0, 4)) | set(range(33, 37)) | set(range(66, 70)) | set(range(99, 103))
    symbols = [[0j] * 4 for _ in range(103)]
    data_symbol = 0
    for sym in range(103):
        if sym in sync_ranges:
            symbols[sym][0] = 1 + 0j
            continue
        b0 = data_bits[2 * data_symbol]
        b1 = data_bits[2 * data_symbol + 1]
        symbols[sym][(b0 << 1) | b1] = 1 + 0j
        data_symbol += 1
    require(data_symbol == 87, "FT4 synthetic data symbol count changed")
    extracted_ranges = [(8, 66), (74, 132), (140, 198)]
    for group_size in (1, 2, 4):
        global_metric = grouped_metrics(symbols, 2, group_size)
        if group_size == 2:
            one = grouped_metrics(symbols, 2, 1)
            global_metric[204:206] = one[204:206]
        elif group_size == 4:
            two = grouped_metrics(symbols, 2, 2)
            one = grouped_metrics(symbols, 2, 1)
            global_metric[200:204] = two[200:204]
            global_metric[204:206] = one[204:206]
        recovered = []
        for a, b in extracted_ranges:
            recovered.extend(global_metric[a:b])
        require(len(recovered) == 174, "FT4 coherent extraction length changed")
        for bit, llr in zip(data_bits, recovered):
            require((llr > 0.0) == (bit == 0),
                    f"FT4 coherent LLR sign/index mismatch for group {group_size}")


def test_ft8_coherent_metric_sign_and_crossing() -> None:
    rng = random.Random(0xF830)
    data_bits = [rng.randrange(2) for _ in range(174)]
    symbols = [[0j] * 8 for _ in range(79)]
    for sym in list(range(0, 7)) + list(range(36, 43)) + list(range(72, 79)):
        symbols[sym][0] = 1 + 0j
    for half, symbol_base in enumerate((7, 43)):
        for local in range(29):
            bit_base = half * 87 + local * 3
            index = (data_bits[bit_base] << 2) | (data_bits[bit_base + 1] << 1) | data_bits[bit_base + 2]
            symbols[symbol_base + local][index] = 1 + 0j
    for group_size in (1, 2, 3):
        recovered = [0.0] * 174
        combo_count = 1 << (3 * group_size)
        for half, symbol_base in enumerate((7, 43)):
            bit_base = half * 87
            bit_end = bit_base + 87
            for local_start in range(0, 29, group_size):
                scores = [0.0] * combo_count
                for combo in range(combo_count):
                    coherent = 0j
                    for g in range(group_size):
                        shift = 3 * (group_size - 1 - g)
                        index = (combo >> shift) & 7
                        coherent += symbols[symbol_base + local_start + g][index]
                    scores[combo] = abs(coherent)
                for bit in range(3 * group_size):
                    out = bit_base + local_start * 3 + bit
                    if out >= bit_end:
                        continue
                    mask = 1 << (3 * group_size - 1 - bit)
                    best0 = max(score for combo, score in enumerate(scores) if not (combo & mask))
                    best1 = max(score for combo, score in enumerate(scores) if combo & mask)
                    recovered[out] = best0 - best1
        for bit, llr in zip(data_bits, recovered):
            require((llr > 0.0) == (bit == 0),
                    f"FT8 coherent LLR sign/index mismatch for group {group_size}")

def test_source_invariants() -> None:
    ghost_hash = hashlib.sha256(extract_ghost_block(CPP).encode()).hexdigest()
    require(ghost_hash == "6e7b4f9c15b21a01f3351442c08c916a7f57f49686a98dfc7acb2296adee1e36",
            f"FT8 ghost-candidate block changed: {ghost_hash}")

    ft8_finder = extract_cpp_function(
        CPP,
        "QVector<Ft8RxDecoder::Candidate> Ft8RxDecoder::findCandidates"
        "(const QVector<double> &samples, double threshold) const",
    )
    ft4_finder = extract_cpp_function(
        CPP,
        "QVector<Ft8RxDecoder::Candidate> Ft8RxDecoder::findFt4Candidates"
        "(const QVector<double> &samples, double threshold) const",
    )
    require(hashlib.sha256(ft8_finder.encode()).hexdigest() ==
            "2d4ca13f3ea884157d8343aaccff16a38b09351ce045bd156ef4f9c1afdc083f",
            "FT8 live candidate finder differs from the validated adaptive-runtime checkpoint")
    require(hashlib.sha256(ft4_finder.encode()).hexdigest() ==
            "3b160cfb8f022a74282c6907a2e470e2615f6792a81bb2af95baa6191fff2af3",
            "FT4 live candidate finder differs from the validated adaptive-runtime checkpoint")
    require("finalizeBaseline" not in ft8_finder,
            "regressing per-tone FT8 temporal baseline is active in live candidate generation")
    require("rescuedCandidate.bucketRescue = true" not in CPP,
            "bucket rescue still displaces validated live candidates")

    require("std::array<int, 103> *decodedTonesOut" in HDR, "FT4 decoded tone output missing")
    require("const std::array<int, 103> &decodedTones" in HDR, "FT4 exact SIC signature missing")
    require("makeFt4ReferenceWaveformRx(decodedTones" in CPP, "FT4 codeword waveform regeneration missing")
    require("constexpr double kFt4SubtractGain = 2.0;" in CPP, "FT4 reference subtraction gain is not 2")

    require(CPP.count("const bool sumProductAllowed = m_offlineAnalysisActive.load();") == 2,
            "FT8/FT4 sum-product is not restricted to controlled offline A/B")
    require("if (!metricDecoded && allowFt8bMetricRecovery &&\n        m_offlineAnalysisActive.load()" in CPP,
            "FT8 coherent path is not restricted to offline A/B")
    require("if (!fecDecoded && m_offlineAnalysisActive.load()" in CPP,
            "FT4 coherent path is not restricted to offline A/B")
    require("for (int offset : {-120, -60, 0, 60, 120})" in CPP,
            "FT8 offline cancellation timing experiment missing")
    require("for (int offset : {-72, -36, 0, 36, 72})" in CPP,
            "FT4 offline cancellation timing experiment missing")

    require("kDecimation = 18" in CPP and "kSamplesPerBasebandSymbol = 32" in CPP,
            "FT4 666.67 Hz complex A/B path missing")
    require("globalCoherentMetric(4" in CPP and "ratioMetric" in CPP,
            "FT4 coherent metric families incomplete")
    require("kDecimation = 60" in CPP and "kBasebandSamplesPerSymbol = 32" in CPP,
            "FT8 200 Hz complex A/B path missing")
    require("coherentMetricRaw(3" in CPP, "FT8 3-symbol coherent metric missing")
    ft8_start = CPP.index("// Deep boundary-only coherent path.")
    ft8_end = CPP.index("if (!metricDecoded) {", ft8_start)
    ft8_section = CPP[ft8_start:ft8_end]
    require("metric4" not in ft8_section, "FT8 deep path incorrectly uses a 4-symbol family")
    require("ldpcDecode174_91SumProduct" in CPP, "sum-product A/B implementation missing")


def main() -> None:
    test_source_invariants()
    test_baseline_fit()
    test_ft4_tone_mapping()
    test_ft4_global_metric_indices()
    test_ft8_group_crossing()
    test_ft4_coherent_metric_sign_and_ranges()
    test_ft8_coherent_metric_sign_and_crossing()
    print("FT8/FT4 wideband algorithm audit: PASS")


if __name__ == "__main__":
    main()
