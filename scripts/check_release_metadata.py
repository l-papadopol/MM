#!/usr/bin/env python3
"""Check all duplicated release identities together, before any platform build."""
from __future__ import annotations

import os
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
LANGS = ("en", "it", "fr", "de", "no", "cs")


def check(root: Path, env: dict[str, str]) -> list[str]:
    errors: list[str] = []

    def read(name: str) -> str:
        try:
            return (root / name).read_text(encoding="utf-8-sig")
        except (OSError, UnicodeError) as exc:
            errors.append(f"{name}: {exc}")
            return ""

    def require(name: str, pattern: str, wanted: str, text: str) -> None:
        values = re.findall(pattern, text, re.MULTILINE)
        if values != [wanted]:
            errors.append(f"{name}: expected {wanted!r}, found {values!r}")

    version = read("MADMODEM_VERSION.txt").strip()
    revision = read("MADMODEM_SOURCE_REVISION.txt").strip()
    print("MadModem CI version guard")
    print(f"  MADMODEM_VERSION.txt : {version}")
    print(f"  SOURCE_REVISION      : {revision}")
    for key, default in (("GITHUB_REF_TYPE", "manual"),
                         ("GITHUB_REF_NAME", "manual"), ("GITHUB_SHA", "manual")):
        print(f"  {key:<20} : {env.get(key, default)}")

    if not re.fullmatch(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)", version):
        errors.append("MADMODEM_VERSION.txt: expected one MAJOR.MINOR.PATCH version")
        return errors
    numbers = version.split(".")
    if not revision.startswith(version + "-") or "\n" in revision:
        errors.append(f"MADMODEM_SOURCE_REVISION.txt: revision does not belong to {version}")
    if env.get("GITHUB_REF_TYPE") == "tag":
        tag = env.get("GITHUB_REF_NAME", "")
        if tag.removeprefix("v") != version:
            errors.append(f"Git tag {tag!r} does not match {version}; use a matching version tag and commit")

    header = read("MadModemVersion.h")
    for macro, wanted in (("STRING", version), ("DISPLAY", f"MadModem {version}"),
                          ("PRERELEASE", "")):
        require(f"MadModemVersion.h / MADMODEM_VERSION_{macro}",
                rf'^\s*#define\s+MADMODEM_VERSION_{macro}\s+"([^"\r\n]*)"\s*$', wanted, header)
    for macro, wanted in zip(("MAJOR", "MINOR", "PATCH"), numbers):
        require(f"MadModemVersion.h / MADMODEM_VERSION_{macro}",
                rf"^\s*#define\s+MADMODEM_VERSION_{macro}\s+(\d+)\s*$", wanted, header)

    cmake = read("CMakeLists.txt")
    require("CMakeLists.txt / project VERSION",
            r"project\s*\(\s*MadModem\s+VERSION\s+(\S+)", version, cmake)
    resource = read("madmodem.rc")
    for field in ("FILEVERSION", "PRODUCTVERSION"):
        require(f"madmodem.rc / {field}", rf"^\s*{field}\s+([0-9,]+)\s*$",
                ",".join(numbers + ["0"]), resource)
    for field in ("FileVersion", "ProductVersion"):
        require(f"madmodem.rc / {field}", rf'VALUE\s+"{field}",\s*"([^"\r\n]*)"',
                version + r"\0", resource)

    try:
        ui = ET.fromstring(read("mainwindow.ui"))
        title = ui.find("./widget/property[@name='windowTitle']/string")
        if title is None or title.text != f"MadModem {version}":
            errors.append(f"mainwindow.ui: windowTitle must be MadModem {version}")
    except ET.ParseError as exc:
        errors.append(f"mainwindow.ui: {exc}")

    for lang in LANGS:
        namespace = f"org.madmodem.mm.{version.replace('.', '')}.{lang}"
        qhp = f"docs/help/MM_{lang}.qhp"
        qhcp = f"docs/help/MM_{lang}.qhcp"
        for name in (qhp, qhcp):
            try:
                project = ET.fromstring(read(name))
                if name == qhp:
                    if project.findtext("namespace") != namespace:
                        errors.append(f"{name}: namespace must be {namespace}")
                    if version not in [n.text for n in project.findall("./filterSection/filterAttribute")]:
                        errors.append(f"{name}: missing version filter {version}")
                elif project.findtext("./assistant/startPage") != f"qthelp://{namespace}/doc/{lang}/index.html":
                    errors.append(f"{name}: start page must use namespace {namespace}")
            except ET.ParseError as exc:
                errors.append(f"{name}: {exc}")
    return errors


def main() -> int:
    errors = check(ROOT, dict(os.environ))
    if errors:
        print(f"ERROR: {len(errors)} release metadata inconsistency/inconsistencies:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        print("Upload the complete contents of one source package to the repository root, "
              "including .github; commit all files together and start a new workflow run.", file=sys.stderr)
        return 1
    print("  status               : OK (header, CMake, Windows resources, UI, six Qt Help projects)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
