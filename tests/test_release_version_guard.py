#!/usr/bin/env python3
"""Run the actual CI guard against clean and deliberately mixed source trees."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
VERSION = (ROOT / "MADMODEM_VERSION.txt").read_text().strip()


class ReleaseGuard(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="mm-version-guard-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        names = ["MADMODEM_VERSION.txt", "MADMODEM_SOURCE_REVISION.txt", "MadModemVersion.h",
                 "CMakeLists.txt", "madmodem.rc", "mainwindow.ui",
                 "scripts/ci_release_version_guard.sh", "scripts/check_release_metadata.py"]
        names += [str(p.relative_to(ROOT)) for ext in ("qhp", "qhcp")
                  for p in (ROOT / "docs/help").glob("MM_*." + ext)]
        for name in names:
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)

    def run_guard(self, **extra):
        env = {k: v for k, v in os.environ.items() if not k.startswith("GITHUB_")}
        env.update(GITHUB_REF_TYPE="branch", GITHUB_REF_NAME="main", GITHUB_SHA="a" * 40)
        env.update(extra)
        return subprocess.run(["bash", str(self.root / "scripts/ci_release_version_guard.sh")],
                              env=env, text=True, capture_output=True)

    def change(self, name, old, new):
        p = self.root / name
        text = p.read_text()
        self.assertIn(old, text)
        p.write_text(text.replace(old, new))

    def test_clean_branch_and_version_tags(self):
        for env in ({}, {"GITHUB_REF_TYPE": "tag", "GITHUB_REF_NAME": "v" + VERSION},
                    {"GITHUB_REF_TYPE": "tag", "GITHUB_REF_NAME": VERSION}):
            with self.subTest(env=env):
                result = self.run_guard(**env)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_original_mixed_version_reports_all_errors(self):
        (self.root / "MADMODEM_VERSION.txt").write_text("0.0.0\n")
        result = self.run_guard()
        self.assertNotEqual(result.returncode, 0)
        for name in ("MADMODEM_SOURCE_REVISION.txt", "MadModemVersion.h", "CMakeLists.txt",
                     "madmodem.rc", "mainwindow.ui", "MM_en.qhp"):
            self.assertIn(name, result.stderr)

    def test_stale_header_is_rejected(self):
        self.change("MadModemVersion.h", f'"{VERSION}"', '"0.0.0"')
        result = self.run_guard()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("MADMODEM_VERSION_STRING", result.stderr)

    def test_stale_numeric_header_is_rejected(self):
        self.change("MadModemVersion.h", "#define MADMODEM_VERSION_PATCH " + VERSION.split(".")[2],
                    "#define MADMODEM_VERSION_PATCH 999")
        self.assertNotEqual(self.run_guard().returncode, 0)

    def test_cmake_windows_ui_and_help_fail_together(self):
        self.change("CMakeLists.txt", "VERSION " + VERSION, "VERSION 0.0.0")
        self.change("madmodem.rc", ",".join(VERSION.split(".") + ["0"]), "0,0,0,0")
        self.change("mainwindow.ui", "MadModem " + VERSION, "MadModem 0.0.0")
        for ext in ("qhp", "qhcp"):
            self.change("docs/help/MM_en." + ext, "org.madmodem.mm." + VERSION.replace(".", ""),
                        "org.madmodem.mm.000")
        result = self.run_guard()
        self.assertNotEqual(result.returncode, 0)
        for name in ("CMakeLists.txt", "madmodem.rc", "mainwindow.ui", "MM_en.qhp", "MM_en.qhcp"):
            self.assertIn(name, result.stderr)

    def test_missing_and_malformed_metadata_are_rejected(self):
        (self.root / "MadModemVersion.h").unlink()
        (self.root / "docs/help/MM_en.qhp").write_text("<broken>")
        result = self.run_guard()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("MadModemVersion.h", result.stderr)
        self.assertIn("MM_en.qhp", result.stderr)

    def test_wrong_tag_is_rejected(self):
        result = self.run_guard(GITHUB_REF_TYPE="tag", GITHUB_REF_NAME="v0.0.0")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Git tag", result.stderr)

    def test_invalid_version_is_rejected(self):
        (self.root / "MADMODEM_VERSION.txt").write_text("invalid\n")
        self.assertNotEqual(self.run_guard().returncode, 0)

    def test_windows_line_endings_are_accepted(self):
        for p in self.root.rglob("*"):
            if p.is_file():
                p.write_bytes(p.read_bytes().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))
        # Bash scripts use LF in git; metadata can legitimately use CRLF.
        for p in (self.root / "scripts").glob("*.sh"):
            p.write_bytes(p.read_bytes().replace(b"\r\n", b"\n"))
        result = self.run_guard()
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
