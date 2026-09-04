#!/usr/bin/env python3
"""Guard the real, low-wear rotator peak-search architecture.

This is intentionally part of the consolidated architecture suite, not a new
CTest.  It protects the operator-facing contract: only implemented algorithms
are exposed, QSO peak tracking is sequential/rate-limited, uses an RX signal
metric independent of modem decoding, and stops before local TX.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
peak_cpp = (ROOT / "rotator" / "RotatorPeakSearch.cpp").read_text(encoding="utf-8")
peak_h = (ROOT / "rotator" / "RotatorPeakSearch.h").read_text(encoding="utf-8")
main_cpp = (ROOT / "mainwindow.cpp").read_text(encoding="utf-8")
settings_h = (ROOT / "settings" / "AppSettings.h").read_text(encoding="utf-8")
dialog_cpp = (ROOT / "dialogs" / "AppSettingsDialog.cpp").read_text(encoding="utf-8")
controller_cpp = (ROOT / "rotator" / "CatRotatorController.cpp").read_text(encoding="utf-8")
dsp_cpp = (ROOT / "dsp" / "DspEngine.cpp").read_text(encoding="utf-8")
dsp_h = (ROOT / "dsp" / "DspEngine.h").read_text(encoding="utf-8")

errors: list[str] = []

def require(cond: bool, msg: str) -> None:
    if not cond:
        errors.append(msg)

# Operator-visible list: exactly the three algorithms that now have real state.
alg_body = peak_cpp.split("QVector<RotatorPeakSearch::AlgorithmInfo> RotatorPeakSearch::algorithms()", 1)[1]
alg_body = alg_body.split("RotatorPeakSearch::AlgorithmInfo RotatorPeakSearch::algorithmById", 1)[0]
for alg in ("pattern-search", "golden-section", "nelder-mead"):
    require(f'QStringLiteral("{alg}")' in alg_body, f"missing implemented peak algorithm {alg}")
for prototype in ("spsa", "extremum-seeking", "bounded-adaptive"):
    require(prototype not in alg_body.lower(), f"prototype/legacy algorithm {prototype} is still exposed to operators")
require('peakSearchAlgorithm = QStringLiteral("pattern-search")' in settings_h,
        "Pattern search is not the safe default")
require("continuousTracking" not in peak_h and "stochastic" not in peak_h,
        "obsolete prototype-algorithm metadata remains in the public peak API")
require("struct Session" in peak_h and "advance(Session *" in peak_h and "reset(Session *" in peak_h,
        "peak algorithms are not represented as one explicit stateful session")
require("GoldenAwaitX1" in peak_h and "NelderReflect" in peak_h and "NelderShrink" in peak_h,
        "golden/Nelder state machine phases are missing")
require("Pattern search (recommended)" in peak_cpp,
        "operator list does not identify the low-wear default")
require("QSO signal peak tracking" in dialog_cpp,
        "rotator settings do not explain that Auto peak also serves QSO tracking")

# QSO target: exact grid-driven, no hidden country centroid and no forced mode.
text_target = main_cpp.split("void MainWindow::updateCatRotatorTextQsoTarget", 1)[1]
text_target = text_target.split("int MainWindow::qsoSignalPeakAudioCenterHz", 1)[0]
require("maidenheadToLonLat" in text_target and "grid" in text_target,
        "text-mode QSO pointing is not grounded in a real Maidenhead locator")
require("setTrackingMode" not in text_target,
        "text-mode QSO target update force-enables tracking instead of respecting operator mode")

# Signal metric must not run an extra FFT/Welch pass in MainWindow. Decoder
# handling comes first; mechanical tracking consumes only a cached metric that
# DspEngine derives asynchronously from the FFT already computed for waterfall.
handle_pos = main_cpp.find("handleRxAudioBlock(block);")
peak_pos = main_cpp.find("updateQsoSignalPeakTracking();", handle_pos)
require(handle_pos >= 0 and peak_pos > handle_pos,
        "QSO peak control runs before live modem handling")
metric_band_body = main_cpp.split("bool MainWindow::qsoSignalPeakMetricBand", 1)[1]
metric_band_body = metric_band_body.split("void MainWindow::configureQsoSignalPeakMetric", 1)[0]
require("radioTelescopeWelchBandPower" not in metric_band_body and "fft" not in metric_band_body.lower(),
        "MainWindow QSO peak metric still performs heavy spectral analysis")
require("configureSignalPeakMetric" in dsp_h and "signalPeakMetricReady" in dsp_h,
        "DSP-worker QSO peak metric API is missing")
require("magnitudes.at(bin)" in dsp_cpp and "signalPower / noisePower" in dsp_cpp,
        "DSP-worker QSO metric does not reuse the existing FFT for signal/noise power")
require("Qt::QueuedConnection" in main_cpp.split("void MainWindow::configureQsoSignalPeakMetric", 1)[1].split("void MainWindow::resetQsoSignalPeakTracking", 1)[0],
        "QSO peak metric configuration is not queued to the DSP worker")
require("kMetricConsumeIntervalMs = 180" in main_cpp,
        "QSO peak metric consumption lost its explicit low-rate limit")

track_body = main_cpp.split("void MainWindow::updateQsoSignalPeakTracking", 1)[1]
track_body = track_body.split("void MainWindow::showAppSettingsDialog", 1)[0]
require("kMinimumCycleIntervalMs = 90000" in track_body,
        "QSO peak tracking lost its 90-second mechanical rate limit")
require("kMaxAzEvaluations = 5" in track_body and "kMaxAltAzEvaluations = 7" in track_body,
        "QSO peak tracking lost its low-wear evaluation caps")
require("hardProbeLifetimeMs" in track_body and "insufficient valid signal samples" in track_body,
        "a silent/intermittent signal can leave a mechanical probe waiting indefinitely")
require("dwellMetricDb < 1.5" in track_body,
        "weak-signal gate missing; the rotator could chase noise")
require("m_qsoPeakCorrectionValid" in track_body,
        "best measured QSO correction is not retained for the active target")

# Local TX owns RF; any mechanical probe must be cancelled before PTT and may
# only return to the QSO target after PTT-off confirmation.
require("void MainWindow::pauseQsoSignalPeakForTransmit" in main_cpp and
        "m_catRotatorController->stop();" in main_cpp,
        "local TX does not explicitly stop an active peak probe")
key_body = main_cpp.split("bool MainWindow::keyPttForTx()", 1)[1].split("void MainWindow::unkeyPttAfterTx()", 1)[0]
require("pauseQsoSignalPeakForTransmit();" in key_body,
        "peak-search pause is not tied to the PTT path")
unkey_body = main_cpp.split("void MainWindow::unkeyPttAfterTx()", 1)[1].split("QString MainWindow::selectedAudioOutputName", 1)[0]
require(re.search(r"if \(pttOffConfirmed\).*?resumeQsoSignalPeakAfterTransmit\(\);", unkey_body, re.S) is not None,
        "QSO target return is not gated by confirmed PTT OFF")

# The controller metadata-only path is the single owner-preserving way to keep
# recurring decoded target updates from yanking the rotor away from a probe.
require("void CatRotatorController::updateQsoTargetMetadata" in controller_cpp,
        "metadata-only QSO target update path is missing")
metadata_body = controller_cpp.split("void CatRotatorController::updateQsoTargetMetadata", 1)[1].split("void CatRotatorController::clearQsoTarget", 1)[0]
require("moveBackend" not in metadata_body and "trackQsoTargetNow" not in metadata_body,
        "metadata-only QSO target update unexpectedly moves the rotator")

if errors:
    print("Rotator peak tracking guard FAILED:", file=sys.stderr)
    for error in errors:
        print(f"- {error}", file=sys.stderr)
    raise SystemExit(1)

print("Rotator peak tracking guard passed: 3 real stateful algorithms, async FFT-reuse QSO metric, low-wear probes and TX interlock are intact.")
