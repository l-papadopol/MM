#!/bin/bash
# Full portable-package update after MadModem exits. All arguments are data.
set -euo pipefail
pid="$1"; target="$2"; archive="$3"; expected="$4"; version="$5"; platform="$6"
build="${7:-0}"
mutating=false
work=""
rollback() {
    status=$?
    if [[ $status != 0 && "$mutating" == true ]]; then
        if mv "$target" "$work/failed-update" && mv "$work/backup" "$target"; then
            echo 'Update failed; original installation restored.'
        else
            echo "Restore could not complete. Preserved files: $work" >&2
        fi
    fi
    if [[ $status != 0 ]]; then
        if [[ "$platform" == mac ]]; then
            osascript -e 'display alert "MadModem update failed. The previous copy has been preserved."' || true
        elif command -v zenity >/dev/null; then
            zenity --error --text="MadModem update failed. The previous copy has been preserved. Log: $work/update.log" || true
        fi
    fi
    exit "$status"
}
trap rollback EXIT
case "$pid" in ''|*[!0-9]*) exit 2;; esac
case "$target" in ''|/|/Applications|/usr|/home) exit 2;; esac
for ((i=0;i<120;i++)); do kill -0 "$pid" 2>/dev/null || break; sleep .5; done
kill -0 "$pid" 2>/dev/null && exit 3
[[ -d "$target" && ! -L "$target" ]] || exit 4
if [[ "$platform" == mac ]]; then
    actual=$(shasum -a 256 "$archive"); actual=${actual%% *}
else
    actual=$(sha256sum "$archive"); actual=${actual%% *}
fi
[[ "$actual" == "$expected" ]] || exit 5
work=$(mktemp -d "${target}.update.XXXXXX")
exec >"$work/update.log" 2>&1
mkdir "$work/stage"
if [[ "$platform" == mac ]]; then
    ditto -xk "$archive" "$work/stage"
    source="$work/stage/MadModem.app"
    executable=Contents/MacOS/MadModem
else
    case "$archive" in
        *.tar.gz) tar -xzf "$archive" -C "$work/stage" ;;
        *.zip) command -v unzip >/dev/null || { echo 'ZIP update requires unzip. Install unzip and retry.' >&2; exit 6; }
               unzip -q "$archive" -d "$work/stage" ;;
        *) echo 'Unsupported portable archive format.' >&2; exit 6 ;;
    esac
    shopt -s nullglob
    roots=("$work/stage"/*)
    [[ ${#roots[@]} == 1 ]] || exit 6
    source="${roots[0]}"
    executable=bin/MadModem
fi
[[ -x "$source/$executable" && -x "$target/$executable" ]] || exit 7
# Release archives must never overwrite user state or change portable mode.
if find "$source" \( -name settings.mad -o -name '*.adi' -o -name '*.adif' -o -name managed-install.flag \) -print | grep -q .; then exit 8; fi
# Verify the distributed bundle before merging it with executable-adjacent
# portable user files, which are intentionally absent from its resource seal.
if [[ "$platform" == mac ]]; then codesign --verify --deep --strict "$source"; fi
cp -a "$target" "$work/backup"
mutating=true
if [[ "$platform" == mac ]]; then ditto "$source" "$target"; else cp -a "$source/." "$target/"; fi
# Bounded startup check. Never wait forever on a broken new executable.
probe_arg=--version
if [[ "$build" != 0 ]]; then probe_arg=--build-info; fi
"$target/$executable" "$probe_arg" >"$work/version.txt" 2>&1 & check_pid=$!
for ((i=0;i<60;i++)); do kill -0 "$check_pid" 2>/dev/null || break; sleep .25; done
if kill -0 "$check_pid" 2>/dev/null; then kill -9 "$check_pid" || true; wait "$check_pid" || true; exit 9; fi
wait "$check_pid"
grep -Fx "MadModem $version" "$work/version.txt"
if [[ "$build" != 0 ]]; then grep -Fx "Build $build" "$work/version.txt"; fi
mutating=false
# Keep the complete previous copy and log for recovery, outside the app folder.
if [[ "$platform" == mac ]]; then open "$target"; else nohup "$target/$executable" >/dev/null 2>&1 </dev/null & fi
