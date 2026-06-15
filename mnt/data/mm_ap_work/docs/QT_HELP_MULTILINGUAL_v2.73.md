# MM multilingual Qt Help - v2.73

MM now treats help content as part of localization rather than as a separate monolingual manual.

## Source layout

```text
docs/help/
  style.css
  en/*.html
  it/*.html
  fr/*.html
  de/*.html
  no/*.html
  cs/*.html
  MM_en.qhp
  MM_it.qhp
  MM_fr.qhp
  MM_de.qhp
  MM_no.qhp
  MM_cs.qhp
```

`cs` is used for Czech because it is the ISO language code used by the existing UI resources; user-facing language labels may still say Czech/CZ.

## Runtime selection

`HelpDialog` reads `UI/language` from `settings.mad`, normalizes it and uses:

1. selected language,
2. English,
3. Italian.

The same fallback chain is used for `.qch` and embedded HTML resources.

## Build

If `qhelpgenerator` is found, CMake builds:

```text
MM_en.qch
MM_it.qch
MM_fr.qch
MM_de.qch
MM_no.qch
MM_cs.qch
```

If not, the application still builds and uses embedded HTML.

## Packaging

`tools/package_mm_zip.sh` copies the localized HTML tree, `.qhp/.qhcp` files and any generated `.qch` files into `MM/help/`.
