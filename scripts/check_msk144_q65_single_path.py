#!/usr/bin/env python3
"""Reject optional/competing MSK144 and Q65 runtime implementations."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


cmake = read("CMakeLists.txt")
main = read("mainwindow.cpp")
q65_decoder = read("modems/q65/Q65Decoder.cpp")
q65_engine = read("modems/q65/Q65NativeEngine.cpp")
q65_tx = read("modems/q65/tx/Q65Transmitter.cpp")
q65_codec = read("third_party/mshv_gpl/port/HvGenQ65/q65_subs.cpp")
q65_generator = read("third_party/mshv_gpl/port/HvGenQ65/gen_q65.cpp")
msk = read("modems/msk144/Msk144Decoder.cpp")
msk_tx = read("modems/msk144/tx/Msk144Transmitter.cpp")
weak_lock = read("modems/weak_signal/WeakSignalCodecLock.cpp")
ft_rx = read("modems/ft8/Ft8RxDecoder.cpp")
ft_tx = read("modems/ft8/tx/Ft8Transmitter.cpp")
q65_test = read("tests/Q65NativeRegression.cpp")
msk_test = read("tests/Msk144NativeRegression.cpp")

checks = {
    "Q65 native engine is mandatory": (
        "modems/q65/Q65NativeEngine.cpp" in cmake
        and "modems/weak_signal/WeakSignalCodecLock.cpp" in cmake
        and "MADMODEM_ENABLE_Q65" not in cmake
        and "decoderq65.cpp" not in cmake
        and "fftw" not in cmake.lower()
    ),
    "Q65 wrapper owns only the native engine": (
        "m_engine.decode(" in q65_decoder
        and "DecoderQ65" not in q65_decoder
        and "ensureMshv" not in q65_decoder
        and "#ifdef MADMODEM_Q65" not in q65_decoder
        and "Q65 RX unavailable" not in main
    ),
    "Q65 RX performs complete protocol validation": (
        "kSyncPositions" in q65_engine
        and "findSyncCandidates" in q65_engine
        and "refineSyncCandidate" in q65_engine
        and "extractSymbolEnergies" in q65_engine
        and "q65_intrinsics_ff" in q65_engine
        and "q65_dec(" in q65_engine
        and "unpack77" in q65_engine
        and "save_hash_call_my_his_r1_r2(configuration.myCall" in q65_engine
        and "save_hash_call_my_his_r1_r2(configuration.dxCall" in q65_engine
    ),
    "Q65 QRA symbols map to transmitted tones 1 through 64": (
        "itone[i]=sent[x] + pp" in q65_generator
        and "if (unpck) pp = 1" in q65_generator
        and "center + (bin - 64 + multiplier) * baud" in q65_engine
        and "part of the protocol mapping" in q65_engine
    ),
    "Q65 RX and TX serialize the one codec workspace": (
        "WeakSignalCodecLock::mutex()" in q65_engine
        and "WeakSignalCodecLock::mutex()" in q65_tx
        and "static std::mutex codecMutex" in weak_lock
        and q65_codec.count("static bool codec_initialized") == 1
        and "static int first=1" not in q65_codec[q65_codec.find("static q65_codec_ds codec"):]
    ),
    "Q65 TX uses direct native-rate synthesis": (
        "generator.genq65itone" in q65_tx
        and "samplesPerSymbol" in q65_tx
        and "generator.genq65(" not in q65_tx
        and "kMshvRate" not in q65_tx
        and "lowLatencyTx() const { return true; }" in q65_tx
        and "Q65Mode::minimumBaseToneHz" in q65_tx
        and "Q65Mode::maximumBaseToneHz" in q65_tx
        and "raisedCosineEdgeGain" in q65_tx
    ),
    "Q65 full AP list spans the protocol report range": (
        "for (int report = -50; report <= 49; ++report)" in q65_engine
        and "q65_dec_fullaplist" in q65_engine
    ),
    "Q65 regression covers A B C D": all(
        f"testSubmode(Q65Mode::Submode::{mode})" in q65_test
        for mode in "ABCD"
    ),
    "MSK144 candidate budget covers the complete period": (
        "const int regions" in msk
        and "energeticStarts" in msk
        and "region * regionSize" in msk
        and "candidates.resize(maxCandidates)" in msk
    ),
    "MSK40 RX and real short-message TX are wired": (
        "decodeMsk40Frame" in msk
        and "bpdecode40" in msk
        and "hash_msk40" in msk
        and "shortMessages, txHz" in main
        and "buildFallbackMskLikeWaveform" not in msk_tx
        and "WeakSignalCodecLock::mutex()" in msk
        and "WeakSignalCodecLock::mutex()" in msk_tx
        and "estimateSyncPhase" in msk
        and "frameSyncMetricAt" in msk
        and "gen.save_hash_call_my_his_r1_r2(m_myCall, 0)" in msk
        and "gen.save_hash_call_my_his_r1_r2(m_dxCall, 1)" in msk
        and "protocolSamplesPerOutputSample" in msk_tx
        and "completeProtocolSamples" in msk_tx
        and "raisedCosineEdgeGain" in msk_tx
        and "MSK40 short-frame round-trip" in msk_test
        and "MSK144 complete-frame TX geometry" in msk_test
        and "MSK40 complete-frame TX geometry" in msk_test
    ),
    "MSK144 target contains every 77-bit protocol primitive": (
        "third_party/mshv_gpl/port/HvPackUnpackMsg/pack_unpack_msg77.cpp" in cmake
        and "madmodem_weak_signal_codec" in cmake
    ),
    "shared message primitives are compiled exactly once": (
        cmake.count("third_party/mshv_gpl/port/HvPackUnpackMsg/pack_unpack_msg77.cpp") == 1
        and cmake.count("third_party/mshv_gpl/nhash.cpp") == 1
        and "target_link_libraries(madmodem_weak_signal_codec PUBLIC ${QT}::Core Threads::Threads)" in cmake
        and "madmodem_q65\n    madmodem_weak_signal_codec" in cmake
    ),
    "FT4 FT8 MSK144 and Q65 serialize the shared message codec": (
        "WeakSignalCodecLock::mutex()" in ft_rx
        and "WeakSignalCodecLock::mutex()" in ft_tx
        and "WeakSignalCodecLock::mutex()" in msk
        and "WeakSignalCodecLock::mutex()" in q65_engine
    ),
    "MSK144 and Q65 use one process-wide codec lock": (
        "Q65CodecLock" not in cmake
        and "Msk144CodecLock" not in cmake
        and "Q65CodecLock" not in q65_engine
        and "Msk144CodecLock" not in msk
    ),
    "MSK144 and Q65 share one complete-frame UTC TX scheduler": (
        "scheduleNativeWeakSignalPeriodTx" in main
        and "handleNativeWeakSignalPeriodTxDue" in main
        and "m_nativeWeakSignalTxBoundaryStart" in main
        and "m_nativeWeakSignalPreparedModulator = buildCurrentTxModulator()" in main
        and "std::move(m_nativeWeakSignalPreparedModulator)" in main
        and "m_chkMsk144TxFirst" in main
        and "m_chkQ65TxFirst" in main
        and "complete frame deferred" in main
    ),
    "native modem regressions are registered": (
        "madmodem_q65_native_regression" in cmake
        and "madmodem_msk144_native_regression" in cmake
    ),
    "native modem targets carry their thread runtime": (
        "target_link_libraries(madmodem_weak_signal_codec PUBLIC ${QT}::Core Threads::Threads)" in cmake
        and "target_link_libraries(madmodem_msk144 PUBLIC madmodem_weak_signal_codec ${QT}::Core ${QT}::Widgets Threads::Threads)" in cmake
        and "target_link_libraries(madmodem_q65 PUBLIC madmodem_weak_signal_codec ${QT}::Core ${QT}::Widgets Threads::Threads)" in cmake
    ),
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"[{'OK' if ok else 'FAIL'}] {name}")

if failed:
    print(f"MSK144/Q65 single-path audit failed: {len(failed)} check(s).", file=sys.stderr)
    raise SystemExit(1)

print("MSK144/Q65 single-path audit passed.")
