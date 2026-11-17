#!/usr/bin/env bash
# File:        build_in_kde_sdk.sh
# Purpose:     Build and test the plugin for Linux inside the Flatpak SDK that
#              decode-orc's Flatpak is built with (org.kde.Sdk, the runtime
#              version in decode-orc's io.github.decode_orc.decode-orc.yml).
#
# Why: the host loads a plugin only when its toolchain tag matches its own
# exactly, and a view only when it was built against the same or an older Qt
# minor. The Flatpak orc-gui is GCC 14 / Qt 6.9 from org.kde.Sdk//6.9, so a
# Linux binary built anywhere else (Nix's GCC 15, a distro's compiler) is
# refused there. Building in the same SDK matches both by construction.
#
# Usage:   scripts/build_in_kde_sdk.sh <decode-orc-checkout> [build-dir]
#
# Needs flatpak with org.kde.Sdk//<version> installed (the version comes from
# ORC_KDE_SDK_VERSION, default 6.9). fmt and spdlog are not in the SDK: the
# same releases the decode-orc Flatpak builds are downloaded here, checked
# against their digests, and built as static, position-independent libraries
# inside the SDK. GoogleTest, for the plugin's tests, is fetched the same way.
# The SDK has FFmpeg, which the functional tests' VBI Source needs.
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <decode-orc-checkout> [build-dir]"
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
sdk_dir="$(cd "$1" && pwd)"
build_dir="${2:-$repo_root/build-kde-sdk}"
mkdir -p "$build_dir"
build_dir="$(cd "$build_dir" && pwd)"
kde_sdk_version="${ORC_KDE_SDK_VERSION:-6.9}"

# The fmt and spdlog modules of decode-orc's Flatpak. Only their headers end up
# in the plugin (it carries a private header-only copy and links neither), so
# matching the Flatpak is tidiness rather than a load requirement.
fmt_url="https://github.com/fmtlib/fmt/archive/refs/tags/10.2.1.tar.gz"
fmt_sha256="1250e4cc58bf06ee631567523f48848dc4596133e163f02615c97f78bab6c811"
spdlog_url="https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz"
spdlog_sha256="1586508029a7d0670dfcb2d97575dcdc242d3868a259742b69f100801ab4e16b"
gtest_url="https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz"
gtest_sha256="7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926"

deps_dir="$build_dir/deps"
mkdir -p "$deps_dir"

fetch() {  # $1 url, $2 sha256, $3 output file
    if [[ ! -f "$3" ]]; then
        curl --fail --location --silent --show-error --output "$3.part" "$1"
        mv "$3.part" "$3"
    fi
    if command -v sha256sum >/dev/null 2>&1; then
        echo "$2  $3" | sha256sum --check --quiet
    else
        echo "$2  $3" | shasum -a 256 --check --quiet
    fi
}

fetch "$fmt_url" "$fmt_sha256" "$deps_dir/fmt.tar.gz"
fetch "$spdlog_url" "$spdlog_sha256" "$deps_dir/spdlog.tar.gz"
fetch "$gtest_url" "$gtest_sha256" "$deps_dir/gtest.tar.gz"

# Everything below runs inside the SDK. Only the plugin checkout, the
# decode-orc checkout and the build directory are shared with it.
flatpak run \
    --filesystem="$repo_root" \
    --filesystem="$sdk_dir:ro" \
    --filesystem="$build_dir" \
    --env=SRC="$repo_root" \
    --env=SDK="$sdk_dir" \
    --env=OUT="$build_dir" \
    --command=bash "org.kde.Sdk//$kde_sdk_version" -euo pipefail -c '
        echo "Compiler: $(gcc --version | head -n 1)"
        prefix="$OUT/deps/prefix"
        jobs="$(nproc)"

        if [[ ! -f "$prefix/lib/cmake/GTest/GTestConfig.cmake" ]]; then
            for dep in fmt spdlog gtest; do
                rm -rf "$OUT/deps/$dep-src" "$OUT/deps/$dep-build"
                mkdir -p "$OUT/deps/$dep-src"
                tar -xzf "$OUT/deps/$dep.tar.gz" -C "$OUT/deps/$dep-src" --strip-components=1
            done
            cmake -S "$OUT/deps/fmt-src" -B "$OUT/deps/fmt-build" -G Ninja \
                -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
                -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_SHARED_LIBS=OFF \
                -DFMT_DOC=OFF -DFMT_TEST=OFF
            cmake --build "$OUT/deps/fmt-build" --parallel "$jobs"
            cmake --install "$OUT/deps/fmt-build"
            cmake -S "$OUT/deps/spdlog-src" -B "$OUT/deps/spdlog-build" -G Ninja \
                -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
                -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DSPDLOG_BUILD_SHARED=OFF \
                -DSPDLOG_BUILD_EXAMPLE=OFF -DSPDLOG_BUILD_TESTS=OFF \
                -DSPDLOG_FMT_EXTERNAL=ON \
                -DCMAKE_PREFIX_PATH="$prefix"
            cmake --build "$OUT/deps/spdlog-build" --parallel "$jobs"
            cmake --install "$OUT/deps/spdlog-build"
            cmake -S "$OUT/deps/gtest-src" -B "$OUT/deps/gtest-build" -G Ninja \
                -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
                -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_SHARED_LIBS=OFF
            cmake --build "$OUT/deps/gtest-build" --parallel "$jobs"
            cmake --install "$OUT/deps/gtest-build"
        fi

        cmake -S "$SRC" -B "$OUT/plugin" -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_PREFIX_PATH="$prefix" \
            -DORC_INTREE_SDK_DIR="$SDK" \
            -DBUILD_TESTS=ON
        cmake --build "$OUT/plugin" --parallel "$jobs"
        ctest --test-dir "$OUT/plugin" --output-on-failure
    '

echo "Built in org.kde.Sdk//$kde_sdk_version: $build_dir/plugin"
