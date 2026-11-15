{
  description = "Decode-Orc teletext and NABTS stage plugin";

  # Keep both nixpkgs inputs on the same releases as the decode-orc flake
  # (github:decode-orc/decode-orc). The host loads a plugin only when the
  # plugin's toolchain tag (compiler family, major version and C++ standard
  # library) equals its own, and a different nixpkgs release brings a
  # different default compiler. On Linux the decode-orc flake builds from
  # nixos-25.05, whose GCC 14 and Qt 6.9 match the released Flatpak
  # (org.kde.Sdk//6.9), so a plugin built here also loads in the Flatpak.
  # macOS and the source tools (clang-format, clang-tidy) come from 26.05.
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    nixpkgs-linux.url = "github:NixOS/nixpkgs/nixos-25.05";
    flake-utils.url = "github:numtide/flake-utils";

    # The decode-orc source tree, used only for its plugin SDK (orc/sdk).
    # Not imported as a flake: the plugin needs the SDK sources, not the
    # host's packages or dev-shell inputs. Move the pin with
    #   nix flake update decode-orc
    decode-orc = {
      url = "github:decode-orc/decode-orc";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, nixpkgs-linux, flake-utils, decode-orc }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        # Source tools: 26.05 on every platform.
        toolPkgs = import nixpkgs { inherit system; };

        # Everything compiled or linked: 25.05 on Linux (the Flatpak's
        # toolchain), 26.05 on macOS.
        pkgs =
          if nixpkgs.lib.hasSuffix "-linux" system
          then import nixpkgs-linux { inherit system; }
          else toolPkgs;

        # The nixpkgs default stdenv, exactly as the decode-orc flake uses:
        # gcc + libstdc++ on Linux, clang + libc++ on macOS. Overriding the
        # compiler here would change the toolchain tag and the host would
        # refuse the binary.
        stdenv = pkgs.stdenv;

        version = "1.0.1";
      in {
        packages.default = stdenv.mkDerivation {
          pname = "orc-plugin-teletext";
          inherit version;
          src = self;

          nativeBuildInputs = with pkgs; [ cmake ninja ];
          # qt6.qtbase for the view binary (orc-view_teletext).
          buildInputs = with pkgs; [ fmt spdlog gtest qt6.qtbase ];
          # A library, not an application: nothing to wrap.
          dontWrapQtApps = true;

          cmakeFlags = [
            "-DORC_INTREE_SDK_DIR=${decode-orc}"
            "-DORC_PLUGIN_VERSION=${version}"
            "-DBUILD_TESTS=ON"
          ];

          doCheck = true;
          checkPhase = ''
            runHook preCheck
            ctest --output-on-failure -L unit
            runHook postCheck
          '';

          meta = {
            description = "Teletext (WST) and NABTS/NAPLPS sink stages for Decode-Orc";
            homepage = "https://github.com/decode-orc/orc-plugin_teletext";
            license = pkgs.lib.licenses.gpl3Plus;
            platforms = pkgs.lib.platforms.unix;
          };
        };

        devShells.default = pkgs.mkShell.override { inherit stdenv; } {
          packages = with pkgs; [
            cmake
            ninja
            pkg-config

            # Third-party headers permitted by the plugin SDK allowlist.
            fmt
            spdlog

            # Unit and functional tests. The functional tests read their
            # captures through decode-orc's VBI Source stage, which needs
            # FFmpeg to unwrap FLAC-compressed captures.
            gtest
            ffmpeg

            # The view binary links Qt 6 Widgets. A release view is built
            # against the view Qt baseline instead (CI installs it; see
            # decode-orc's plugin-sdk.md), since orc-gui refuses a view built
            # for a newer Qt minor than its own.
            qt6.qtbase

            # Development tools.
            toolPkgs.clang-tools
            ccache
            git
            python3
          ] ++ pkgs.lib.optionals pkgs.stdenv.isLinux [
            gdb
          ];

          shellHook = ''
            echo "orc-plugin_teletext nix development environment"
            # Probe via $CXX, not a hardcoded g++: the Darwin stdenv is
            # clang-based and has no g++.
            echo "C++ toolchain: $($CXX --version | head -n1)"

            # The pinned decode-orc SDK. CMakeLists.txt uses it when
            # -DORC_INTREE_SDK_DIR is not given; pass -DORC_INTREE_SDK_DIR to
            # build against a local decode-orc checkout instead.
            export ORC_INTREE_SDK_DIR="${decode-orc}"

            export CMAKE_CXX_COMPILER_LAUNCHER=ccache
          '';

          CMAKE_EXPORT_COMPILE_COMMANDS = 1;
          # Default to Ninja when no -G is given, matching decode-orc.
          CMAKE_GENERATOR = "Ninja";
        };
      }
    );
}
