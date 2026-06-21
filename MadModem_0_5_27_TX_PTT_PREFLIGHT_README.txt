MadModem 0.5.27 - TX/PTT preflight popups
================================================

Base used in this package:
- MadModem_0_5_25_fldigi_textmode_core_pass_FULL_SOURCE(2).zip as uploaded in this chat.

Purpose:
- TX/PTT must never fail silently. If the operator cannot transmit because a required station/PTT/CAT check fails, MadModem now shows a warning dialog and offers to open the relevant Settings page.

Main changes:
1. Missing station identity is explicit.
   - If My Call is empty, TX/PTT/Tune/PTT test is blocked with a popup.
   - If My Locator is empty or invalid, TX/PTT/Tune/PTT test is blocked with a popup.
   - The popup can open Settings -> User/QTH directly.

2. CAT/Hamlib PTT failures are explicit.
   - If CAT/Hamlib PTT is selected but the controller is unavailable, TX is blocked with a popup.
   - If the radio rejects the PTT/TX command, TX is blocked with a popup including Hamlib lastStatus() when available.
   - The popup can open Audio/PTT + CAT settings.

3. Serial RTS/DTR PTT failures are explicit.
   - If RTS/DTR PTT is selected but no port is configured, TX is blocked with a popup.
   - If the serial PTT port cannot be opened, TX is blocked with a popup.
   - If RTS/DTR cannot be asserted, TX is blocked with a popup.
   - Audio-only TX remains possible only when PTT method is explicitly set to None.

4. Audio TX start failures are explicit.
   - If the selected TX audio device cannot start, MadModem shows a popup instead of only writing to the log.

5. PTT TEST is explicit too.
   - RX-active, TX-active, CAT/Hamlib, serial-port, and disabled-PTT test failures now show a popup instead of only logging.

Files changed:
- mainwindow.cpp
- mainwindow.h
- MadModemVersion.h
- CMakeLists.txt
- THIRD_PARTY_NOTICES.md
- translations/ui_en.ini
- translations/ui_it.ini
- translations/ui_fr.ini
- translations/ui_de.ini
- translations/ui_no.ini
- translations/ui_cs.ini

Notes:
- This is a UX/safety patch only. It does not change the FT8/FT4 decoder, text-mode DSP, CW decoder, scheduler, CAT model list, or Hamlib low-level implementation.
- I could not perform a full compile in the sandbox because Qt5 development CMake files are not installed here. CMake stops at missing Qt5Config.cmake before compiling project sources.
