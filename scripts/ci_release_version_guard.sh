#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION_FILE="$ROOT_DIR/MADMODEM_VERSION.txt"
HEADER_FILE="$ROOT_DIR/MadModemVersion.h"

if [[ ! -f "$VERSION_FILE" ]]; then
    echo "ERROR: missing MADMODEM_VERSION.txt" >&2
    exit 1
fi

expected="$(tr -d '\r\n[:space:]' < "$VERSION_FILE")"
if [[ -z "$expected" ]]; then
    echo "ERROR: MADMODEM_VERSION.txt is empty" >&2
    exit 1
fi

ref_name="${GITHUB_REF_NAME:-manual}"
ref_type="${GITHUB_REF_TYPE:-manual}"
sha="${GITHUB_SHA:-manual}"
normalized_tag="${ref_name#v}"

printf 'MadModem CI version guard\n'
printf '  MADMODEM_VERSION.txt : %s\n' "$expected"
printf '  GITHUB_REF_TYPE      : %s\n' "$ref_type"
printf '  GITHUB_REF_NAME      : %s\n' "$ref_name"
printf '  GITHUB_SHA           : %s\n' "$sha"

# On tagged release builds the tag is the source of truth for what the user
# expects to publish.  The source tree version must match the selected tag.
# This catches the common GitHub mistake where main was updated but the tag
# still points to an older commit, producing plain 0.5.77 packages.
if [[ "$ref_type" == "tag" ]]; then
    if [[ "$normalized_tag" != "$expected" ]]; then
        cat >&2 <<ERR
ERROR: selected Git tag does not match source version.
  tag, normalized  : $normalized_tag
  source version   : $expected

Fix:
  delete and recreate the tag on the commit that contains MADMODEM_VERSION.txt=$expected,
  then rerun the distribution workflow.
ERR
        exit 1
    fi
fi

if [[ ! -f "$HEADER_FILE" ]]; then
    echo "ERROR: missing MadModemVersion.h" >&2
    exit 1
fi
if ! grep -Fq "#define MADMODEM_VERSION_STRING \"$expected\"" "$HEADER_FILE"; then
    echo "ERROR: MadModemVersion.h does not contain MADMODEM_VERSION_STRING \"$expected\"" >&2
    grep -n "MADMODEM_VERSION_STRING" "$HEADER_FILE" >&2 || true
    exit 1
fi

# CMake project(VERSION) is numeric by design, so CMAKE_PROJECT_VERSION may be
# 0.5.77.  The full release identity is MADMODEM_PACKAGE_VERSION / this file.
printf '  status               : OK\n'
