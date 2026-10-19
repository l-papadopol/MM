# MadModem 0.6.4 translation maintenance

The six runtime dictionaries are English, Italian, French, German, Norwegian and Czech. Complete sentences are translated by stable key; protocol names and units remain shared where appropriate. Do not construct translated prose by replacing individual English words.

`tools/update_ui_translations.py` harvests first-party UI strings and preserves reviewed translations. Add exact overrides there when wording or placeholders change. `tools/audit_localization.py` checks key parity/order, placeholders, duplicates and unintended language mixing.

All twelve help topics in each language are maintained in `tools/update_help_pages.py`. Regenerate the 72 HTML pages after editing it. Qt Help namespaces, filters and footer versions follow the public application version. The embedded HTML remains available if Qt Help generation is unavailable.

```bash
python3 tools/update_ui_translations.py
python3 tools/update_help_pages.py
python3 tools/audit_localization.py
python3 tools/audit_documentation.py
```

Generated audit logs and compiled help caches belong outside the source tree. A dictionary audit checks structure; it does not replace review of wording or testing on the native platform.
