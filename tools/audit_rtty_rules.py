#!/usr/bin/env python3
"""Static validator for MadModem's external rtty_rules file.

The binary deliberately contains no contest-name branches. This audit checks that a
future edited rules file only uses schema features supported by the generic engine.
"""
from __future__ import annotations
import json
import re
import sys
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[1]
RULES = ROOT / "rtty_rules"

ALLOWED_STATUS = {"active", "cancelled", "inactive"}
ALLOWED_DUPE_SCOPE = {"overall", "band", "period", "band_period"}
ALLOWED_MULT_SCOPE = {"overall", "band", "period", "band_period"}
ALLOWED_AGGREGATE = {"mults", "continents"}
ALLOWED_MULT_SOURCES = {
    "dxcc", "continent", "cq_zone", "call", "call_area", "wpx_prefix",
    "call_regex_capture", "dxcc_or_call_area",
}
ALLOWED_CONDITION_KEYS = {
    "any", "all", "not", "same_country", "same_continent", "band_in",
    "own_primary_prefix_in", "not_own_primary_prefix_in",
    "dx_primary_prefix_in", "not_dx_primary_prefix_in",
    "own_continent_in", "dx_continent_in",
    "own_country_name_regex", "not_own_country_name_regex",
    "dx_country_name_regex", "not_dx_country_name_regex",
    "own_call_regex", "not_own_call_regex",
    "dx_call_regex", "not_dx_call_regex",
    "field_equals_own", "field_own_regex", "field_rx_regex",
}
FORMULA_VARS = {"QSO", "POINTS", "MULTS", "CONTINENTS"}

errors: list[str] = []
warnings: list[str] = []

def err(where: str, msg: str) -> None:
    errors.append(f"{where}: {msg}")

def warn(where: str, msg: str) -> None:
    warnings.append(f"{where}: {msg}")

def validate_regex(where: str, pattern: str) -> None:
    try:
        re.compile(pattern)
    except re.error as exc:
        err(where, f"invalid regex {pattern!r}: {exc}")

def validate_condition(where: str, obj) -> None:
    if not obj:
        return
    if not isinstance(obj, dict):
        err(where, "condition must be an object")
        return
    for key, value in obj.items():
        if key not in ALLOWED_CONDITION_KEYS:
            err(where, f"unsupported condition key {key!r}")
            continue
        if key in {"any", "all"}:
            if not isinstance(value, list) or not value:
                err(where, f"{key} must be a non-empty array")
            else:
                for i, child in enumerate(value):
                    validate_condition(f"{where}.{key}[{i}]", child)
        elif key == "not":
            validate_condition(f"{where}.not", value)
        elif key.endswith("_regex") and key not in {"field_own_regex", "field_rx_regex"}:
            if not isinstance(value, str) or not value:
                err(where, f"{key} must be a regex string")
            else:
                validate_regex(f"{where}.{key}", value)
        elif key in {"field_own_regex", "field_rx_regex"}:
            if not isinstance(value, dict) or not value.get("id") or not value.get("pattern"):
                err(where, f"{key} needs id and pattern")
            else:
                validate_regex(f"{where}.{key}.pattern", value["pattern"])

def validate_fields(where: str, fields, serial_enabled: bool) -> set[str]:
    ids: set[str] = set()
    if not isinstance(fields, list):
        err(where, "fields must be an array")
        return ids
    for i, field in enumerate(fields):
        fw = f"{where}[{i}]"
        if not isinstance(field, dict):
            err(fw, "field must be an object")
            continue
        fid = str(field.get("id", "")).strip().upper()
        if not fid:
            err(fw, "missing id")
            continue
        if fid in ids:
            err(fw, f"duplicate field id {fid}")
        ids.add(fid)
        regex = field.get("regex")
        if regex:
            validate_regex(f"{fw}.regex", regex)
        validate_condition(f"{fw}.when", field.get("when", {}))
        if fid == "SERIAL" and not serial_enabled:
            err(fw, "SERIAL field present but serial.enabled is false")
    return ids

def valid_url(url: str) -> bool:
    try:
        parsed = urlparse(url)
        return parsed.scheme in {"http", "https"} and bool(parsed.netloc)
    except Exception:
        return False

try:
    root = json.loads(RULES.read_text(encoding="utf-8"))
except Exception as exc:
    print(f"ERROR: cannot parse {RULES}: {exc}", file=sys.stderr)
    sys.exit(2)

if root.get("schema") != 1:
    err("root", f"schema must be 1, got {root.get('schema')!r}")
if not root.get("updated_utc"):
    err("root", "missing updated_utc")
profiles = root.get("profiles")
if not isinstance(profiles, list) or not profiles:
    err("root", "profiles must be a non-empty array")
    profiles = []

seen: set[str] = set()
active = 0
for idx, profile in enumerate(profiles):
    where = f"profiles[{idx}]"
    if not isinstance(profile, dict):
        err(where, "profile must be an object")
        continue
    pid = str(profile.get("id", "")).strip().lower()
    if not pid:
        err(where, "missing id")
        pid = f"#{idx}"
    if pid in seen:
        err(where, f"duplicate profile id {pid}")
    seen.add(pid)
    where = f"profile[{pid}]"
    if not profile.get("name"):
        err(where, "missing name")
    status = str(profile.get("status", "active")).lower()
    if status not in ALLOWED_STATUS:
        err(where, f"invalid status {status}")
    if status == "active":
        active += 1
    bands = profile.get("bands", [])
    if status == "active" and not bands:
        err(where, "active profile has no bands")
    if len(bands) != len(set(bands)):
        err(where, "duplicate bands")
    dupe_scope = str(profile.get("dupe_scope", "band")).lower()
    if dupe_scope not in ALLOWED_DUPE_SCOPE:
        err(where, f"invalid dupe_scope {dupe_scope}")
    periods = profile.get("periods", [])
    if (dupe_scope in {"period", "band_period"}) and not periods:
        err(where, "period-based dupe scope requires periods")
    period_ids: set[str] = set()
    for i, period in enumerate(periods):
        pw = f"{where}.periods[{i}]"
        if not isinstance(period, dict):
            err(pw, "period must be an object")
            continue
        p_id = str(period.get("id", "")).strip()
        if not p_id or not period.get("start_utc") or not period.get("end_utc"):
            err(pw, "period requires id/start_utc/end_utc")
        if p_id in period_ids:
            err(pw, f"duplicate period id {p_id}")
        period_ids.add(p_id)

    serial = profile.get("serial", {})
    serial_enabled = bool(serial.get("enabled", False))
    if serial_enabled:
        if int(serial.get("start", 0)) < 1:
            err(where, "serial start must be >= 1")
        if not (1 <= int(serial.get("width", 0)) <= 6):
            err(where, "serial width must be 1..6")

    exchange = profile.get("exchange", {})
    sent_ids = validate_fields(f"{where}.exchange.sent", exchange.get("sent", []), serial_enabled)
    recv_ids = validate_fields(f"{where}.exchange.received", exchange.get("received", []), serial_enabled)

    macros = profile.get("macros", [])
    if status == "active" and not macros:
        err(where, "active profile has no macros")
    for i, macro in enumerate(macros):
        mw = f"{where}.macros[{i}]"
        if not isinstance(macro, dict) or not macro.get("label") or "text" not in macro:
            err(mw, "macro requires label and text")

    score = profile.get("scoring", {})
    formula = str(score.get("formula", "POINTS * MULTS")).upper()
    if not re.fullmatch(r"[A-Z0-9_+*/(). \t-]+", formula):
        err(where, f"formula contains unsupported characters: {formula!r}")
    for token in re.findall(r"[A-Z_]+", formula):
        if token not in FORMULA_VARS:
            err(where, f"formula uses unsupported variable {token}")
    for i, qrule in enumerate(score.get("qso_points", [])):
        qw = f"{where}.scoring.qso_points[{i}]"
        if not isinstance(qrule, dict):
            err(qw, "QSO point rule must be an object")
            continue
        validate_condition(f"{qw}.when", qrule.get("when", {}))
        method = qrule.get("method")
        if method and method != "distance_km":
            err(qw, f"unsupported scoring method {method!r}")
        if not method and "points" not in qrule:
            err(qw, "missing points or method")

    for i, mult in enumerate(score.get("multipliers", [])):
        mw = f"{where}.scoring.multipliers[{i}]"
        source = str(mult.get("source", "")).lower()
        if not (source in ALLOWED_MULT_SOURCES or source.startswith("field:")):
            err(mw, f"unsupported multiplier source {source!r}")
        if source.startswith("field:"):
            fid = source.split(":", 1)[1].upper()
            if fid not in recv_ids:
                warn(mw, f"multiplier reads RX field {fid} not declared in received exchange")
        scope = str(mult.get("scope", "band")).lower()
        if scope not in ALLOWED_MULT_SCOPE:
            err(mw, f"unsupported scope {scope!r}")
        if scope in {"period", "band_period"} and not periods:
            err(mw, "period-scoped multiplier requires periods")
        aggregate = str(mult.get("aggregate", "mults")).lower()
        if aggregate not in ALLOWED_AGGREGATE:
            err(mw, f"unsupported aggregate {aggregate!r}")
        validate_condition(f"{mw}.when", mult.get("when", {}))
        if source == "call_regex_capture":
            opts = mult.get("options", {})
            pattern = opts.get("pattern") if isinstance(opts, dict) else None
            if not pattern:
                err(mw, "call_regex_capture requires options.pattern")
            else:
                validate_regex(f"{mw}.options.pattern", pattern)

    source = profile.get("source", {})
    url = source.get("url", "") if isinstance(source, dict) else ""
    if status == "active" and not valid_url(url):
        err(where, "active profile requires a valid source.url")
    if not profile.get("cabrillo_id"):
        warn(where, "missing cabrillo_id")

print(f"rtty_rules: {len(profiles)} profiles, {active} active, schema={root.get('schema')}, updated={root.get('updated_utc')}")
for item in warnings:
    print("WARNING:", item)
for item in errors:
    print("ERROR:", item, file=sys.stderr)
if errors:
    print(f"FAILED: {len(errors)} error(s), {len(warnings)} warning(s)", file=sys.stderr)
    sys.exit(1)
print(f"PASS: 0 errors, {len(warnings)} warning(s)")
