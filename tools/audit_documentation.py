#!/usr/bin/env python3
"""Validate MadModem release metadata and multilingual help sources."""
from __future__ import annotations

from html.parser import HTMLParser
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

from update_help_pages import PAGES as REVIEWED_HELP_PAGES

ROOT = Path(__file__).resolve().parents[1]
HELP = ROOT / "docs" / "help"
LANGS = ["en", "it", "fr", "de", "no", "cs"]
VERSION = (ROOT / "MADMODEM_VERSION.txt").read_text(encoding="utf-8").strip()


class HtmlCheck(HTMLParser):
    pass


def main() -> int:
    errors: list[str] = []
    expected_namespace = f"org.madmodem.mm.{VERSION.replace('.', '')}"

    current_docs = [
        ROOT / "README.md",
        ROOT / "CHANGELOG.md",
        ROOT / "RELEASE_NOTES.md",
        ROOT / "TRANSLATION_AUDIT.md",
        ROOT / "docs" / "README.md",
        ROOT / "docs" / "VERSIONING.md",
    ]
    for path in current_docs:
        if VERSION not in path.read_text(encoding="utf-8"):
            errors.append(f"{path.relative_to(ROOT)}: current version {VERSION} is not mentioned")

    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    for required in (
        "Digital modes, station control and logging in one desktop application.",
        "## What you can do",
        "## Operating views",
        "## Languages and help",
    ):
        if required not in readme:
            errors.append(f"README.md: missing public overview section/text {required!r}")

    public_readme_forbidden = (
        "optional backend",
        "development areas",
        "under active development",
        "should always be validated",
        "checkpoint",
        "regression",
    )
    for phrase in public_readme_forbidden:
        if phrase.casefold() in readme.casefold():
            errors.append(f"README.md: internal or unfinished-feature wording leaked into public copy: {phrase!r}")

    release_notes = (ROOT / "RELEASE_NOTES.md").read_text(encoding="utf-8")
    if re.search(r"\b(?:checkpoint|branch|superseded)\b", release_notes, re.IGNORECASE):
        errors.append("RELEASE_NOTES.md: contains internal development-history wording")

    qrc = ET.parse(ROOT / "resources.qrc")
    qrc_aliases = {node.get("alias") for node in qrc.findall(".//file")}

    for lang in LANGS:
        lang_dir = HELP / lang
        pages = sorted(lang_dir.glob("*.html"))
        if len(pages) != 12:
            errors.append(f"help/{lang}: expected 12 HTML pages, found {len(pages)}")
        for page in pages:
            text = page.read_text(encoding="utf-8")
            try:
                parser = HtmlCheck()
                parser.feed(text)
                parser.close()
            except Exception as exc:
                errors.append(f"{page.relative_to(ROOT)}: HTML parse error: {exc}")
            if text.count("<body>") != 1 or text.count("</body>") != 1:
                errors.append(f"{page.relative_to(ROOT)}: malformed or duplicate body element")
            if text.count("<html ") != 1 or text.count("</html>") != 1:
                errors.append(f"{page.relative_to(ROOT)}: malformed or duplicate html element")
            if not text.rstrip().endswith("</body></html>"):
                errors.append(f"{page.relative_to(ROOT)}: content exists outside the body/html close")
            if f"MadModem {VERSION}" not in text:
                errors.append(f"{page.relative_to(ROOT)}: missing current footer version")
            for href in re.findall(r"href=['\"]([^'\"]+)['\"]", text):
                if href.startswith(("http:", "https:", "#")):
                    continue
                if not (page.parent / href).resolve().exists():
                    errors.append(f"{page.relative_to(ROOT)}: broken link {href}")
            alias = f"{lang}/{page.name}"
            if alias not in qrc_aliases:
                errors.append(f"resources.qrc: missing embedded help alias {alias}")

            reviewed = REVIEWED_HELP_PAGES.get(lang, {}).get(page.name)
            if reviewed is not None:
                reviewed_title, reviewed_body = reviewed
                if f"<title>{reviewed_title}</title>" not in text or reviewed_body not in text:
                    errors.append(f"{page.relative_to(ROOT)}: reviewed localized body is stale")

        qhp = HELP / f"MM_{lang}.qhp"
        qhcp = HELP / f"MM_{lang}.qhcp"
        try:
            project = ET.parse(qhp)
            collection = ET.parse(qhcp)
        except Exception as exc:
            errors.append(f"help/{lang}: Qt Help XML parse error: {exc}")
            continue
        namespace = project.getroot().findtext("namespace") or ""
        wanted_namespace = f"{expected_namespace}.{lang}"
        if namespace != wanted_namespace:
            errors.append(f"{qhp.name}: namespace {namespace!r}, expected {wanted_namespace!r}")
        attributes = [n.text or "" for n in project.getroot().findall("./filterSection/filterAttribute")]
        if VERSION not in attributes:
            errors.append(f"{qhp.name}: missing filter attribute {VERSION}")
        for node in project.getroot().findall(".//*[@ref]"):
            ref = node.get("ref")
            if ref and not (HELP / ref).exists():
                errors.append(f"{qhp.name}: broken ref {ref}")
        start_page = collection.getroot().findtext("./assistant/startPage") or ""
        if wanted_namespace not in start_page:
            errors.append(f"{qhcp.name}: start page does not use {wanted_namespace}")

    root_point_notes = sorted(ROOT.glob("*_0_5_*lab*.md")) + sorted(ROOT.glob("*_experimental*.md"))
    if root_point_notes:
        errors.append("historical point/lab notes remain in package root: " + ", ".join(p.name for p in root_point_notes))

    if errors:
        print("Documentation audit FAILED:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print(f"Documentation audit PASSED: version {VERSION}, 72 localized HTML pages and six Qt Help projects are consistent.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
