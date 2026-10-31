#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
assets_dir="$(cd "${1:?Expected directory containing downloaded packages}" && pwd)"
: "${GH_TOKEN:?Missing GitHub token}"
: "${GH_REPO:?Missing GitHub repository}"
: "${GITHUB_SHA:?Missing build commit}"
: "${GITHUB_RUN_NUMBER:?Missing workflow run number}"
bash "$root/scripts/ci_release_version_guard.sh"
version="$(tr -d '\r\n' < "$root/MADMODEM_VERSION.txt")"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]

# Collect the complete distribution set; do not publish logs or a partial release.
shopt -s nullglob
publish_dir="$(mktemp -d)"
trap 'rm -rf "$publish_dir"' EXIT
packages=()
old_names=()
require_package() {
    local download_name="$1"
    shift
    if [[ $# != 1 || ! -s "${1:-}" ]]; then
        echo "ERROR: missing, empty or ambiguous release package ($*)." >&2
        exit 1
    fi
    # Rename only the publication copy; packaging and installer checks keep their
    # established input paths. Download names consistently describe their purpose.
    cp -- "$1" "$publish_dir/$download_name"
    packages+=("$publish_dir/$download_name")
    old_names+=("${1##*/}")
}
for platform in Ubuntu24.04 Fedora43; do
    for extension in zip tar.gz; do
        require_package "MadModem-$version-Linux-$platform-64bit-Portable.$extension" \
            "$assets_dir/MadModem-$version-$platform-x86_64.$extension"
    done
done
require_package "MadModem-$version-Linux-Ubuntu24.04-64bit-Installer.run" \
    "$assets_dir"/MadModem-"$version"-*-Ubuntu24.04-amd64-Setup.run
require_package "MadModem-$version-Linux-Fedora43-64bit-Installer.run" \
    "$assets_dir"/madmodem-"$version"-*.fc43.x86_64-Setup.run
require_package "MadModem-$version-Linux-Ubuntu24.04-64bit-Package.deb" \
    "$assets_dir"/MadModem-"$version"-*-Ubuntu24.04-amd64.deb
require_package "MadModem-$version-Linux-Fedora43-64bit-Package.rpm" \
    "$assets_dir"/madmodem-"$version"-*.fc43.x86_64.rpm
for variant in legacy avx2; do
    cpu=Compatible
    [[ "$variant" != avx2 ]] || cpu=AVX2-FMA
    require_package "MadModem-$version-Windows-64bit-Portable-$cpu.zip" \
        "$assets_dir/MadModem-$version-Windows-x86_64-$variant-standalone.zip"
done
require_package "MadModem-$version-Windows-64bit-Installer-AutoCPU.exe" \
    "$assets_dir/MadModem-$version-Windows-x64-Setup.exe"
for arch in x86_64 arm64; do
    cpu=Intel
    [[ "$arch" != arm64 ]] || cpu=AppleSilicon
    for extension in zip dmg pkg; do
        case "$extension" in zip) kind=Portable ;; dmg) kind=DiskImage ;; pkg) kind=Installer ;; esac
        require_package "MadModem-$version-macOS-$cpu-$kind.$extension" \
            "$assets_dir/MadModem-$version-macOS-$arch-unsigned.$extension"
    done
done
(
    cd "$publish_dir"
    for package in "${packages[@]}"; do sha256sum "${package##*/}"; done > SHA256SUMS.txt
)
packages+=("$publish_dir/SHA256SUMS.txt")

prerelease=false
create_options=()
edit_options=()
if [[ "${GITHUB_REF_TYPE:-}" == tag ]]; then
    tag="$GITHUB_REF_NAME"
    title="MadModem $version"
    create_options+=(--verify-tag)
else
    # Unique build tags never move an existing version tag or replace a stable release.
    tag="build-$version-$GITHUB_RUN_NUMBER"
    title="MadModem $version — build $GITHUB_RUN_NUMBER"
    prerelease=true
    create_options+=(--latest=false)
    edit_options+=(--latest=false)
fi

notes_file="$publish_dir/release-notes.md"
python3 "$root/scripts/stamp_release_notes.py" "$root/RELEASE_NOTES.md" "$notes_file" "$GITHUB_RUN_NUMBER" "$GITHUB_SHA"

# New releases stay drafts until every asset has uploaded. Re-runs resume drafts
# or replace same-named assets on an existing mutable release, preserving its notes.
if ! gh release view "$tag" --json id >/dev/null 2>&1; then
    gh release create "$tag" --draft --target "$GITHUB_SHA" \
        --title "$title" --notes-file "$notes_file" \
        --prerelease="$prerelease" "${create_options[@]}"
fi
gh release upload "$tag" "${packages[@]}" --clobber
# Remove only superseded names from this version, after every replacement has
# uploaded successfully. Preserve custom attachments and assets of other versions.
remote_names="$(gh release view "$tag" --json assets --jq '.assets[].name')"
while IFS= read -r asset; do
    obsolete=false
    for old_name in "${old_names[@]}"; do
        if [[ "$asset" == "$old_name" ]]; then obsolete=true; break; fi
    done
    # Linux installer names previously included the source revision, which may
    # differ from the revision being uploaded now.
    case "$asset" in
        MadModem-"$version"-*-Ubuntu24.04-amd64-Setup.run|madmodem-"$version"-*.fc43.x86_64-Setup.run)
            obsolete=true ;;
    esac
    if [[ "$obsolete" == true ]]; then gh release delete-asset "$tag" "$asset" --yes; fi
done <<< "$remote_names"
# Keep the operator's notes and refresh only our machine identity, also on
# same-tag rebuilds. Every platform binary carries this workflow's build number.
gh release view "$tag" --json body --jq .body > "$publish_dir/existing-notes.md"
python3 "$root/scripts/stamp_release_notes.py" "$publish_dir/existing-notes.md" "$notes_file" "$GITHUB_RUN_NUMBER" "$GITHUB_SHA"
gh release edit "$tag" --draft=false --prerelease="$prerelease" --notes-file "$notes_file" "${edit_options[@]}"
release_url="$(gh release view "$tag" --json url --jq .url)"
printf 'Published downloads: %s\n' "$release_url"
if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
    printf 'Download installers and portable packages: [%s](%s)\n' "$tag" "$release_url" >> "$GITHUB_STEP_SUMMARY"
fi
