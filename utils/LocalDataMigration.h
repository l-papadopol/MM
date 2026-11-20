#pragma once
/*
 * IC8TEM edition: user data in the folder of the installation.
 *
 * The Windows installer of the edition puts "local-data.flag" next to the
 * executable: settings, logbook, downloaded lists and diagnostics then live in
 * "<installation>/data" (see MadModemInstallation::localDataRoot()). On the first
 * start this brings there, once, what MadModem kept in the per-user folders of the
 * system (on Windows %LOCALAPPDATA%\MadModem and %APPDATA%\MadModem): moved when the
 * original MadModem is not installed, copied when it is, so that it keeps its own.
 *
 * Copyright (C) 2026 Costantino Cerrotta, IC8TEM. GNU GPL v3 or later.
 */
#include <QString>

namespace MadModemInstallation {
/** Returns a line for the runtime log, empty when there was nothing to do. */
QString migrateToLocalData();
}
