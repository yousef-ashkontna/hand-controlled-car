# Contributing

Keep sender application code in `src/sender/`, receiver code in `src/receiver/`, reusable ESP-IDF components in `components/`, and supporting documentation in `docs/`.

Before proposing a change:

1. Run `python scripts/test_host.py` and build both environments with `pio run`.
2. Update wiring or protocol documentation when changing pins, packet layout, units, or control behavior.
3. Describe what was tested on hardware separately from compile-only checks.
4. Keep generated files, local editor settings, credentials, and tool installations out of commits.

For bug reports, include the firmware role, board model, platform version, reproduction steps, and relevant serial output.

Never commit include/pairing_config.h or firmware binaries containing pairing keys. CI uses generated ephemeral keys with synthetic MAC addresses solely to exercise the configured build path.
