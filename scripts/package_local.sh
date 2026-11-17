#!/usr/bin/env bash
#
# Package the built plugin as a release asset and emit the per-platform
# release-manifest fragment that goes with it.
#
# Since host ABI 12 every plugin release must publish an
# orc-plugin-manifest.yaml declaring each binary's platform, host ABI,
# toolchain tag and SHA-256 digest; a release without one cannot be browsed,
# installed, or updated to. This script writes one platform's worth of that
# manifest; scripts/merge_manifests.sh combines the per-platform fragments
# into the single asset published with the release.
#
# Usage: package_local.sh <build-dir> [output-dir]

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <build-dir> [output-dir]"
    exit 1
fi

build_dir="$1"
output_dir="${2:-dist}"

mkdir -p "$output_dir"

case "$(uname -s)" in
    Linux)
        platform="linux"
        ext="so"
        exe_ext=""
        ;;
    Darwin)
        platform="macos"
        ext="dylib"
        exe_ext=""
        ;;
    MINGW*|MSYS*|CYGWIN*)
        platform="windows"
        ext="dll"
        exe_ext=".exe"
        ;;
    *)
        echo "Unsupported platform"
        exit 1
        ;;
esac

# --- Build facts, straight from the compiled descriptor -------------------

build_info="$(find "$build_dir" -type f -name "orc-plugin-build-info${exe_ext}" | head -n 1)"

if [[ -z "$build_info" ]]; then
    echo "Could not locate orc-plugin-build-info in $build_dir"
    echo "(build the project first; it is built unconditionally)"
    exit 1
fi

plugin_id=""
plugin_version=""
stage_name=""
abi=""
toolchain_tag=""

# orc-plugin-build-info writes its lines with "\n", but a Windows build prints
# them through a text-mode stdout, so every value would otherwise arrive with a
# trailing CR and end up embedded in this platform's manifest fragment. That
# difference is invisible in the YAML and only surfaces when merge_manifests.sh
# compares the Windows fragment against the Linux one at release time.

view_abi=""
view_qt=""
while IFS='=' read -r key value; do
    case "$key" in
        plugin_id)      plugin_id="$value" ;;
        plugin_version) plugin_version="$value" ;;
        stage_name)     stage_name="$value" ;;
        abi)            abi="$value" ;;
        toolchain_tag)  toolchain_tag="$value" ;;
        view_abi)       view_abi="$value" ;;
        view_qt)        view_qt="$value" ;;
    esac
done < <("$build_info" | tr -d '\r')

for required in plugin_id plugin_version stage_name abi toolchain_tag; do
    if [[ -z "${!required}" ]]; then
        echo "orc-plugin-build-info did not report $required"
        exit 1
    fi
done

# --- Copy the binary under the release-asset name -------------------------
#
# orc-plugin_<stage-name>_<platform>[_abi<N>].<ext> is the artifact naming
# convention the host validates downloads against. The _abi<N> token lets one
# release carry builds for several host ABIs side by side.

plugin_name="orc-plugin_${stage_name}_${platform}_abi${abi}.${ext}"

candidate="$(find "$build_dir" -type f \( -name "*orc-stage-plugin-teletext*.${ext}" -o -name "orc-stage-plugin-teletext.${ext}" \) | head -n 1)"

if [[ -z "$candidate" ]]; then
    echo "Could not locate built plugin binary in $build_dir"
    exit 1
fi

cp "$candidate" "$output_dir/$plugin_name"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
bash "$script_dir/check_release_dependencies.sh" stage "$output_dir/$plugin_name"

# --- Digest ---------------------------------------------------------------

digest() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{print $1}'
    else
        echo "Neither sha256sum nor shasum is available; cannot digest $1" >&2
        exit 1
    fi
}

sha256="$(digest "$output_dir/$plugin_name")"

# --- The view binary, when it was built ------------------------------------
#
# orc-view_<name>_<platform>_view<N>.<ext>: the orc-view_ prefix is required
# (hosts tell a view from a stage binary by it); the rest mirrors the stage
# binary's name, with the view ABI in place of the host ABI.

view_name=""
view_sha256=""
if [[ -n "$view_abi" ]]; then
    view_candidate="$(find "$build_dir" -type f -name "orc-view_teletext.${ext}" | head -n 1)"
    if [[ -z "$view_candidate" ]]; then
        echo "The build reports a view (view_abi=$view_abi) but orc-view_teletext.${ext} is not in $build_dir"
        exit 1
    fi
    view_name="orc-view_${stage_name}_${platform}_view${view_abi}.${ext}"
    cp "$view_candidate" "$output_dir/$view_name"
    bash "$script_dir/check_release_dependencies.sh" view "$output_dir/$view_name"
    view_sha256="$(digest "$output_dir/$view_name")"
fi

# --- Manifest fragment ----------------------------------------------------
#
# A complete, valid single-platform manifest: publishing a one-platform
# release can use it as-is. Fragment names deliberately do not start with
# orc-plugin_, so CI's release-asset glob never picks them up directly.

manifest_fragment="$output_dir/plugin-manifest-${platform}.yaml"

# Schema 2 adds the view entry (kind: view). It follows the stage entry for
# the same platform, so hosts that predate schema 2, which take the first
# entry for their platform, still install the stage binary.
manifest_schema=1
if [[ -n "$view_name" ]]; then
    manifest_schema=2
fi

cat > "$manifest_fragment" <<EOF
# Generated by scripts/package_local.sh - do not edit by hand.
manifest_schema: ${manifest_schema}
plugin_id: ${plugin_id}
plugin_version: ${plugin_version}
artifacts:
  - file: ${plugin_name}
    platform: ${platform}
    abi: ${abi}
    toolchain_tag: ${toolchain_tag}
    sha256: ${sha256}
EOF


if [[ -n "$view_name" ]]; then
    cat >> "$manifest_fragment" <<EOF
  - file: ${view_name}
    kind: view
    platform: ${platform}
    abi: ${view_abi}
    qt: "${view_qt}"
    toolchain_tag: ${toolchain_tag}
    sha256: ${view_sha256}
EOF
    echo "Packaged: $output_dir/$view_name"
fi

echo "Packaged: $output_dir/$plugin_name"
echo "Manifest fragment: $manifest_fragment"
