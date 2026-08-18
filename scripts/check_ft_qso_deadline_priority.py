#!/usr/bin/env python3
"""Source guard for the single-path FT8 active-QSO deadline policy."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "modems/ft8/Ft8RxDecoder.cpp").read_text(encoding="utf-8")
HDR = (ROOT / "modems/ft8/Ft8RxDecoder.h").read_text(encoding="utf-8")
MAIN = (ROOT / "mainwindow.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)
    print(f"PASS: {message}")


def function_body(signature: str) -> str:
    start = CPP.index(signature)
    brace = CPP.index("{", start)
    depth = 0
    for index in range(brace, len(CPP)):
        if CPP[index] == "{":
            depth += 1
        elif CPP[index] == "}":
            depth -= 1
            if depth == 0:
                return CPP[start:index + 1]
    raise AssertionError(f"unterminated function: {signature}")


def main() -> None:
    decode = function_body(
        "QVector<Ft8RxDecoder::Decode> Ft8RxDecoder::decodeSlot"
        "(const QVector<double> &samples,"
    )
    finder = function_body(
        "QVector<Ft8RxDecoder::Candidate> Ft8RxDecoder::findCandidates"
        "(const QVector<double> &samples, double threshold) const"
    )

    require("bool qsoPriority = false;" in HDR,
            "candidate carries an explicit QSO-priority classification")
    require("t_currentFtWorkClass == MadModemRuntime::WorkClass::FtGate" in finder,
            "priority admission is restricted to the live gate")
    require("m_qsoDeadlineActive.load(std::memory_order_acquire)" in finder,
            "priority admission requires an active QSO")
    require("constexpr double kQsoFocusHalfSpanHz = 90.0;" in finder,
            "QSO search is bounded around the tracked correspondent")
    require("constexpr int kQsoPriorityCandidateLimit = 32;" in finder,
            "QSO candidate work has a hard upper bound")
    require("qsoPriority ? qMin(kSyncMin, 0.92) : kSyncMin" in finder,
            "only the reserved QSO quota receives the lower sync floor")
    require("normalCandidateCount >= kMaxCandidates" in finder,
            "reserved candidates do not consume the normal wideband quota")

    focused = decode.index("const QVector<CandidateDecode> qsoPairs = decodeCandidateSet")
    wideband = decode.index("QVector<CandidateDecode> passPairs = decodeCandidateSet")
    require(focused < wideband,
            "QSO candidates enter LDPC before the general wideband candidates")
    require("&qsoWorkers,\n                                                                              true" in decode,
            "the bounded QSO set receives metric recovery")
    require("&workersThisPass, false" in decode,
            "the ordinary live gate keeps its established fast policy")
    require("qsoMyCall.isEmpty() || messageMentionsCall(pair.decode.message, qsoMyCall)" in decode,
            "early completion validates both correspondent and local calls")
    require("wideband gate deferred" in decode and "break;" in decode[focused:wideband],
            "a recovered QSO reply returns without waiting for wideband LDPC")
    require("std::async" not in decode and "QtConcurrent" not in decode,
            "no competing decoder path was introduced")

    require(MAIN.count('"setQsoDeadlineActive"') >= 8 and MAIN.count('"setDxCall"') >= 6,
            "MainWindow propagates QSO lifecycle and callsign context")
    require('"setRxMarkerHz"' in MAIN,
            "the decoder receives the tracked correspondent frequency")

    print("FT8 QSO deadline priority source audit: PASS")


if __name__ == "__main__":
    main()
