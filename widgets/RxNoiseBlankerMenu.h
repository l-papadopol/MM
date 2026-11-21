#pragma once
/*
 * Decode menu entries for the RX noise blanker and the MSK144 carrier notch (IC8TEM).
 * Copyright (C) 2026 Costantino Cerrotta, IC8TEM. GNU GPL v3 or later.
 */
#include <functional>

class QMenu;
class QString;

namespace MadModemNoise {
/** Loads the saved settings into the decoders (call once at start-up). */
void loadSettings();
/**
 * Adds "Noise Blanker" (Off / NB2 Auto / NB2, threshold) and "MSK144 Carrier Notch (ZAP)"
 * to the Decode menu. The settings are saved and take effect at the next audio block.
 */
void installMenu(QMenu *decodeMenu, const std::function<void(const QString &)> &log);
} // namespace MadModemNoise
