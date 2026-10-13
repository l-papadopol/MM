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
packages=()
require_package() {
    if [[ $# != 1 || ! -s "${1:-}" ]]; then
        echo "ERROR: missing, empty or ambiguous release package ($*)." >&2
        exit 1
    fi
    packages+=("$1")
}
for platform in Ubuntu24.04 Fedora43; do
    require_package "$assets_dir/MadModem-$version-$platform-x86_64.zip"
    require_package "$assets_dir/MadModem-$version-$platform-x86_64.tar.gz"
done
require_package "$assets_dir"/MadModem-"$version"-*-Ubuntu24.04-amd64-Setup.run
require_package "$assets_dir"/madmodem-"$version"-*.fc43.x86_64-Setup.run
for variant in legacy avx2; do
    require_package "$assets_dir/MadModem-$version-Windows-x86_64-$variant-standalone.zip"
done
require_package "$assets_dir/MadModem-$version-Windows-x64-Setup.exe"
for arch in x86_64 arm64; do
    for extension in zip dmg pkg; do
        require_package "$assets_dir/MadModem-$version-macOS-$arch-unsigned.$extension"
    done
done
(
    cd "$assets_dir"
    for package in "${packages[@]}"; do sha256sum "${package##*/}"; done > SHA256SUMS.txt
)
packages+=("$assets_dir/SHA256SUMS.txt")

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

# New releases stay drafts until every asset has uploaded. Re-runs resume drafts
# or replace same-named assets on an existing mutable release, preserving its notes.
if ! gh release view "$tag" --json id >/dev/null 2>&1; then
    gh release create "$tag" --draft --target "$GITHUB_SHA" \
        --title "$title" --notes-file "$root/RELEASE_NOTES.md" \
        --prerelease="$prerelease" "${create_options[@]}"
fi
gh release upload "$tag" "${packages[@]}" --clobber
gh release edit "$tag" --draft=false --prerelease="$prerelease" "${edit_options[@]}"
release_url="$(gh release view "$tag" --json url --jq .url)"
printf 'Published downloads: %s\n' "$release_url"
if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
    printf 'Download installers and portable packages: [%s](%s)\n' "$tag" "$release_url" >> "$GITHUB_STEP_SUMMARY"
fi
