#!/usr/bin/env python3
"""Run consolidated MadModem static/release guard suites.

CTest intentionally registers a small number of stable suites.  The individual
checks remain separate scripts so they can still be run directly while working
on one subsystem, but CI reports one result per responsibility instead of one
result per historical bug.
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SUITES: dict[str, list[tuple[str, str]]] = {
    "architecture": [
        ("py", "scripts/check_runtime_hardening.py"),
        ("py", "scripts/check_cat_ft_band_sync.py"),
        ("py", "scripts/check_msk144_q65_single_path.py"),
    ],
    "ui": [
        ("py", "scripts/check_rtty_contest_layout.py"),
        ("py", "scripts/check_rtty_live_contest_runtime.py"),
        ("py", "scripts/check_cw_macro_ui.py"),
        ("py", "scripts/check_ui_theme_integrity.py"),
    ],
    "ft": [
        ("py", "scripts/check_ft_atomic_tx_lifecycle.py"),
        ("py", "scripts/check_ft_caller_queue_waterfall_restore.py"),
        ("py", "scripts/check_ft_linear_sequencer.py"),
        ("py", "scripts/check_ft_qso_deadline_priority.py"),
        ("unix", "scripts/check_ft4_adaptive_runtime.sh"),
        ("unix", "scripts/check_ft_wideband_sensitivity.sh"),
    ],
    "release": [
        ("py", "scripts/check_operator_ui_copy.py"),
        ("py", "tools/audit_localization.py"),
        ("py", "tools/audit_documentation.py"),
        ("pyarg", "tools/audit_rtty_rules.py|rtty_rules"),
        ("pyarg", "tools/audit_rtty_rules.py|cw_rules"),
        ("unix", "scripts/ci_release_version_guard.sh"),
    ],
    "waterfall": [
        ("unix", "scripts/check_waterfall_wsjtx_flattening.sh"),
    ],
}


def command_for(kind: str, spec: str, with_unix: bool) -> list[str] | None:
    if kind == "py":
        return [sys.executable, str(ROOT / spec)]
    if kind == "pyarg":
        script, arg = spec.split("|", 1)
        return [sys.executable, str(ROOT / script), arg]
    if kind == "unix":
        if not with_unix:
            return None
        bash = shutil.which("bash")
        if not bash:
            raise RuntimeError(f"bash is required for {spec}")
        return [bash, str(ROOT / spec)]
    raise RuntimeError(f"unknown guard kind: {kind}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("suite", choices=sorted(SUITES))
    parser.add_argument("--with-unix-checks", action="store_true")
    args = parser.parse_args()

    checks = SUITES[args.suite]
    ran = 0
    skipped = 0
    print(f"MadModem consolidated guard suite: {args.suite}")

    for kind, spec in checks:
        cmd = command_for(kind, spec, args.with_unix_checks)
        label = spec.split("|", 1)[0]
        if cmd is None:
            skipped += 1
            print(f"\n--- SKIP {label} (Unix-only) ---")
            continue
        ran += 1
        print(f"\n--- RUN {label} ---", flush=True)
        completed = subprocess.run(cmd, cwd=ROOT)
        if completed.returncode != 0:
            print(
                f"\nConsolidated {args.suite} suite FAILED in {label} "
                f"(exit {completed.returncode}).",
                file=sys.stderr,
            )
            return completed.returncode

    print(
        f"\nConsolidated {args.suite} suite PASSED: "
        f"{ran} check(s) run, {skipped} platform check(s) skipped."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
