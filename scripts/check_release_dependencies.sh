#!/usr/bin/env bash
#
# Fail when a release binary needs a shared library a user's machine may not
# have, or may have at another version.
#
# orc-gui loads a plugin into its own process: anything the plugin links,
# beyond the system libraries (and, for a view, Qt), has to be found on the
# user's machine at a compatible version. A plugin built against the decode-orc
# SDK carries its own spdlog and fmt (decode-orc's plugin-sdk.md, "Private spdlog
# and fmt"), so neither may appear here. This check would have stopped
# decode-orc issue #391, macOS binaries that named spdlog by a /nix/store path.
#
# On Linux and macOS it also fails a binary that exports spdlog or fmt symbols:
# exported, the plugin's copy binds to the host's (or the host's to the
# plugin's), and the two only agree while their versions match exactly.
#
# Usage: check_release_dependencies.sh <stage|view> <binary>

set -euo pipefail

if [[ $# -ne 2 || ( "$1" != "stage" && "$1" != "view" ) ]]; then
    echo "Usage: $0 <stage|view> <binary>" >&2
    exit 2
fi

kind="$1"
binary="$2"
bad=()

case "$(uname -s)" in
    Linux)
        # The C/C++ runtime, plus Qt and GL for a view.
        allowed='^(libc|libm|libdl|libpthread|librt|libstdc\+\+|libgcc_s|ld-linux[^.]*)\.so'
        if [[ "$kind" == "view" ]]; then
            allowed="${allowed}|^(libQt6[A-Za-z]+|libGL|libGLX|libOpenGL|libEGL)\\.so"
        fi
        while read -r needed; do
            [[ "$needed" =~ $allowed ]] || bad+=("$needed")
        done < <(objdump -p "$binary" | awk '$1 == "NEEDED" { print $2 }')
        exported="$(nm -D -C --defined-only "$binary" | grep -c -E ' (spdlog|fmt)::' || true)"
        ;;
    Darwin)
        # otool -L lists the binary's own install name first.
        allowed='^(/usr/lib/|/System/Library/)'
        if [[ "$kind" == "view" ]]; then
            allowed="${allowed}|^@rpath/Qt[A-Za-z]+\\.framework/"
        fi
        own_id="$(otool -D "$binary" | tail -n +2 | head -n 1)"
        while read -r needed; do
            [[ "$needed" == "$own_id" ]] && continue
            [[ "$needed" =~ $allowed ]] || bad+=("$needed")
        done < <(otool -L "$binary" | tail -n +2 | awk '{ print $1 }')
        # Mangled names carry the length-prefixed namespaces: 6spdlog, and
        # 3fmt followed by fmt's versioned inline namespace (3fmt3v12...).
        # nm -gU lists the defined external symbols.
        exported="$(nm -gU "$binary" | grep -c -E '6spdlog|3fmt[0-9]+v[0-9]' || true)"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        # The Windows system DLL set is large and varies with the toolchain,
        # so look only for the libraries the SDK compiles in privately.
        if command -v objdump >/dev/null 2>&1; then
            deps="$(objdump -p "$binary" | awk '/DLL Name:/ { print $3 }')"
        elif command -v dumpbin >/dev/null 2>&1; then
            deps="$(dumpbin //DEPENDENTS "$binary" | grep -i '\.dll' | awk '{ print $1 }')"
        else
            echo "No objdump or dumpbin: dependencies of $binary not checked"
            exit 0
        fi
        while read -r needed; do
            [[ -z "$needed" ]] && continue
            if [[ "$needed" =~ ^(spdlog|fmt) ]]; then
                bad+=("$needed")
            fi
        done <<< "$deps"
        ;;
    *)
        echo "Unsupported platform" >&2
        exit 2
        ;;
esac

if [[ "${exported:-0}" -gt 0 ]]; then
    bad+=("${exported} exported spdlog/fmt symbols (link with orc_add_stage_plugin(), or orc_sdk_hide_private_logging_symbols() for a view)")
fi

if [[ ${#bad[@]} -gt 0 ]]; then
    echo "$binary is not self-contained:" >&2
    printf '  %s\n' "${bad[@]}" >&2
    exit 1
fi
echo "Dependencies OK: $binary"
