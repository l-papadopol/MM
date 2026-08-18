#!/usr/bin/env python3
"""Static release guard for the 0.5.8 runtime-safety corrections."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


main = read("mainwindow.cpp")
audio = read("audio/AudioEngine.cpp")
tx_audio = read("audio/TxAudioEngine.cpp")
cat = read("rig/HamlibController.cpp")
q65 = read("modems/q65/Q65Decoder.cpp")
cmake = read("CMakeLists.txt")
logbook = read("logbook/AdifLogbook.cpp")
map_source = read("widgets/QsoMapWidget.cpp")
wav_reader = read("audio/WavFileReader.cpp")

cat_ptt = cat[
    cat.index("bool HamlibController::setWsjtLikeCatPtt"):
    cat.index("bool HamlibController::setHamlibPttMode")
]
cat_disconnect = cat[
    cat.index("bool HamlibController::disconnectRig"):
    cat.index("void HamlibController::pollNow")
]

checks = {
    "one strict WAV reader rejects malformed layouts": (
        "RIFF size exceeds file bounds" in wav_reader
        and "inconsistent WAV block alignment or byte rate" in wav_reader
        and "invalid or non-finite sample" in wav_reader
        and "tests/WavFileReaderTest.cpp" in cmake
        and "bool parseWavHeader(QFile &file, WavStreamFormat" not in main
        and main.count("MadModemAudio::parseWavHeader(file, wav, errorMessage)") == 2
    ),
    "bounded UI audio relay": (
        "BoundedAudioDispatcher" in main
        and "takePending(2" in main
        and "RX overload protection" in main
    ),
    "audio capture has its own thread": (
        "m_audioEngine->moveToThread(m_audioThread)" in main
        and "m_audioThread->start()" in main
    ),
    "FT input still bypasses MainWindow": (
        "m_ft8RxDecoder, &Ft8RxDecoder::processAudioBlock" in main
        and "FT live audio bypasses MainWindow completely" in main
    ),
    "FT worker configuration lock cannot self-deadlock": (
        "std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);" in read("modems/ft8/Ft8RxDecoder.cpp")
        and "decodeOut.slotPeriodMs = currentSlotMs();" not in read("modems/ft8/Ft8RxDecoder.cpp")
    ),
    "audio buffer removal is batched": (
        "int consumedBytes = 0" in audio
        and "m_pendingBytes.remove(0, consumedBytes)" in audio
    ),
    "configured RX/TX devices never fall back silently": (
        "automatic device fallback is disabled" in audio
        and "automatic device fallback is disabled" in tx_audio
        and "!defaultRequested && !exactDeviceMatch" in audio
        and "!defaultRequested && !exactDeviceMatch" in tx_audio
    ),
    "CAT route is fail closed": (
        "RIG_PTT_ON_DATA" in cat_ptt
        and "return setHamlibPttMode(true, RIG_PTT_ON," not in cat_ptt.split("if (rearData)", 1)[1].split("}", 1)[0]
    ),
    "CAT disconnect never invents a PTT-OFF acknowledgement": (
        "const bool pttOffConfirmed" in cat_disconnect
        and "emit pttChanged(false)" not in cat_disconnect
        and "if (pttOffConfirmed)" in cat_disconnect
    ),
    "Q65 RX feature is truthful": (
        "bool Q65Decoder::fullRxAvailable()" in q65
        and "Q65 RX start blocked" in main
        and "Q65 RX will be unavailable" in cmake
    ),
    "ADIF persistence is atomic": (
        "QSaveFile" in logbook
        and "writeLogbookAtomically" in logbook
    ),
    "offline image WAV loop is non-reentrant": "QCoreApplication::processEvents" not in main,
    "OSM TLS remains verified": "ignoreSslErrors" not in map_source,
    "OSM tile responses are bounded": "madmodemTileOversize" in map_source,
    "Hamlib configure has no default build side effect": (
        "option(MADMODEM_AUTOBUILD_HAMLIB" in cmake
        and "Automatically build bundled Hamlib" in cmake
        and "\n    OFF)" in cmake[cmake.index("option(MADMODEM_AUTOBUILD_HAMLIB"):]
    ),
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"[{'OK' if ok else 'FAIL'}] {name}")

if failed:
    print(f"Runtime hardening audit failed: {len(failed)} check(s).", file=sys.stderr)
    raise SystemExit(1)

print("Runtime hardening audit passed.")
