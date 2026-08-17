# Hamlib notice

MadModem includes the Hamlib source tree so CAT/PTT builds can be reproduced
without downloading an extra archive. The bundled source lives in:

    third_party/hamlib_lgpl/source

Hamlib is the Ham Radio Control Libraries project.

Copyright and attribution, as stated by upstream Hamlib:

- Copyright (C) 2000-2012 The Hamlib Group
- Copyright (C) 2000,2001 Frank Singleton
- Copyright (C) 2000-2011 Stephane Fillod
- Additional authors are listed in the upstream `AUTHORS` file.

License summary:

- Hamlib frontend/backend library source is LGPL-2.1-or-later.
- Some upstream supplied programs/examples are GPL-2.0-or-later.
- The copied license files are preserved here as `COPYING_HAMLIB_LGPL.txt`,
  `COPYING_HAMLIB_GPL.txt`, and `LICENSE_HAMLIB.txt`.

MadModem release builds link to the Hamlib library API by default. If Hamlib is not built
or installed, the normal all-in-one build fails rather than silently compiling the
safe development stub. The stub path remains available only for explicit developer
builds with MADMODEM_REQUIRE_HAMLIB=OFF.
