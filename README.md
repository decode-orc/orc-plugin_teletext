# orc-plugin_teletext

Teletext and NABTS stage plugin for [Decode-Orc](https://github.com/decode-orc/decode-orc).

One shared library, plugin id `org.decodeorc.stage.teletext`, registers two
stages that moved here from decode-orc core
([issue #377](https://github.com/decode-orc/decode-orc/issues/377)):

- **Teletext Sink** (`teletext_sink`) — recovers World System Teletext
  (ITU-R BT.653 System B) from the VBI of a 625- or 525-line source, writes
  the packet stream, and browses the pages it found.
- **NABTS Sink** (`nabts_sink`) — recovers North American Basic Teletext
  (ITU-R BT.653 System C, CEA-516) from a 525-line source, writes the packet
  stream, interprets its NAPLPS presentation layer, and browses the records
  and pages it found.

Both stages ride the same WST/NABTS line-slicing and Hamming 8/4 decoding
chain, built here as `orc-vbi-services` (`src/vbi-services/`) and shared by
the two stage implementations rather than duplicated.

See [`docs/teletext-sink.md`](docs/teletext-sink.md) and
[`docs/nabts-sink.md`](docs/nabts-sink.md) for what each stage does and every
parameter it takes — the same text the GUI's Help dialog shows via each
stage's `instructions.md`.

## SDK Contract (Decode-Orc 2.x — host ABI 20 / plugin API 4)

- Include only the public SDK umbrella header in plugin/stage code:

```cpp
#include <orc/abi/orc_plugin_sdk.h>
```

  The SDK is tiered: `orc/abi/` holds the frozen binary contract (descriptor,
  entrypoints, registration, services), `orc/stage/` the stage interfaces and
  data types, and `orc/support/` the header/helper conveniences.

- `src/plugin.cpp` exports the two required entrypoints
  (`orc_get_stage_plugin_descriptor`, `orc_register_stage_plugin`) once, for
  both stages, via `ORC_DEFINE_STAGE_PLUGIN(descriptor, TeletextSinkStage,
  NabtsSinkStage)`.

- The descriptor is built with `ORC_STAGE_PLUGIN_DESCRIPTOR`, which fills in
  the host ABI version, plugin API version and toolchain tag from the SDK the
  plugin is compiled against. The host requires all three to match exactly at
  load time.

- `src/vbi-services/` is **not** part of the SDK contract. These sources
  (teletext/NABTS line slicing, page and record decoding) were public SDK
  headers under `orc/support/` in decode-orc until they moved out of the SDK —
  see the comment at the top of `src/vbi-services/CMakeLists.txt` for why.
  They are plugin-local code shared between `teletext_sink` and `nabts_sink`,
  not an API third-party plugins can rely on.

- Each stage implements `ICatalogueResults`
  (`<orc/stage/tooling/catalogue_results.h>`) and declares
  `StageToolKind::CatalogueBrowser` so the host's generic catalogue browser can
  present its pages/records; neither uses the removed format-specific
  analysis-sink result types.

## Instructions-per-stage

One shared library registers two stages, so the SDK's
`ORC_STAGE_INSTRUCTIONS_MD` macro — which reads `<library>.md` — can't tell
them apart. Both stages instead use
`ORC_TELETEXT_STAGE_INSTRUCTIONS_MD(stage_name)` from
`src/common/stage_instructions_file.h`, which reads
`<library>.<stage_name>.md`. `CMakeLists.txt` copies each stage's
`instructions.md` to that name next to the built and installed library
(`liborc-stage-plugin-teletext.teletext_sink.md`,
`liborc-stage-plugin-teletext.nabts_sink.md`).

## Plugin Version Source

- The plugin descriptor version is set at CMake configure time.
- Default value is `project(VERSION ...)` from `CMakeLists.txt`.
- In GitHub Actions tag builds, tags like `v1.2.3` are automatically mapped to
  plugin version `1.2.3`.
- Override manually with `-DORC_PLUGIN_VERSION=1.2.3`.

## Building a Core (Bundled) Copy

decode-orc can build this repository as a submodule under
`external/plugins/orc-plugin_teletext`, bundling the stages back into the host
distribution while they still live in this separate repository. That build
passes `-DORC_TELETEXT_AS_CORE_PLUGIN=ON`, which sets the descriptor's
`is_core_plugin` flag so the host treats the bundled copy as a core plugin
(no trust prompt, hidden from `plugins list` by default) and does not build
`orc-plugin-build-info` (the bundled copy is never the subject of a release
manifest). Release builds for the Plugin Manager leave this option off
(the default).

## Local Build (against in-tree decode-orc SDK)

1. Clone decode-orc and this repository as sibling directories.
2. Enter the Nix development shell (recommended):

```bash
nix develop
```

   `flake.nix` pins `nixpkgs` to the same release as decode-orc's own flake
   and tracks a pinned `decode-orc` commit for its SDK sources only (never
   built or linked as a flake dependency — just `orc/sdk/`). This matters:
   since Decode-Orc 2.0 the loader requires the plugin's toolchain tag
   (compiler family + major version + standard library, e.g.
   `gcc15/libstdc++`) to equal the host's exactly, and rejects the plugin
   otherwise. If you build the host from a different environment, build the
   plugin from that same environment.

   The dev shell exports `ORC_INTREE_SDK_DIR` pointing at the flake's pinned
   decode-orc checkout, so a bare `cmake -S . -B build` inside it already
   resolves the SDK. Pass `-DORC_INTREE_SDK_DIR=/path/to/decode-orc`
   explicitly to build against a local checkout instead (for example a
   decode-orc branch you are also changing).

3. Configure and build. `cmake/DecodeOrcInTreeSDK.cmake` builds the SDK
   targets (the SDK headers plus the `orc-sdk-support` static library, from
   `orc/sdk/src` in the decode-orc checkout) with no host library built or
   linked:

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build --parallel
```

4. Run the tests:

```bash
ctest --test-dir build --output-on-failure -L unit         # SDK-only, always runs
ctest --test-dir build --output-on-failure -L functional    # skips without captures
```

   The functional tests read real WST and NABTS captures through decode-orc's
   VBI Source stage, compiled from the same decode-orc checkout as the SDK
   into a test-only library (it needs FFmpeg, which the dev shell provides).
   The captures are not checked in; pass
   `-DORC_TELETEXT_TEST_DATA_DIR=<dir>` pointing at a directory laid out like
   decode-orc's `test-data/` (`teletext/bt8x8 sample/...`,
   `teletext/TBS Electra WST 525-60/...`,
   `teletext/NTSC NABTS Teletext samples/...`,
   `teletext/cx2388x NTSC sample/...`). Each test skips when its capture is
   absent, so CI builds them but skips them.

5. Package the local artifact and its manifest fragment:

```bash
./scripts/package_local.sh build dist
```

### Building with `nix build`

```bash
nix build .#default
```

builds the plugin against the `decode-orc` input pinned in `flake.lock` and
runs the unit tests (not the functional tests, which need the reference
captures) as the package's check phase.

## Local Build (installed SDK)

1. Install decode-orc (or at minimum its plugin SDK package) to a prefix.
2. Configure and build this plugin against that install prefix:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/absolute/path/to/decode-orc-install
cmake --build build --parallel
```

## Artifact Naming Contract

Release assets and local packaging output follow
`orc-plugin_<name>_<platform>[_abi<N>].<ext>`:

- `orc-plugin_teletext_linux_abi20.so`
- `orc-plugin_teletext_macos_abi20.dylib`
- `orc-plugin_teletext_windows_abi20.dll`

The name token is `teletext` (the plugin), not a stage name — this binary
registers two stages. The `_abi<N>` token records the host ABI the binary
targets, so one release can carry builds for several host ABIs side by side.
`package_local.sh` takes the ABI number from the built descriptor, so the name
always matches the binary.

## Release Manifest (required)

Every release must publish an `orc-plugin-manifest.yaml` asset alongside its
binaries. A release without one — or with an invalid one — cannot be browsed,
installed, or updated to: the host resolves which artifact to install
entirely from the manifest, and refuses anything whose declared ABI or
toolchain tag cannot work.

```yaml
manifest_schema: 1
plugin_id: org.decodeorc.stage.teletext
plugin_version: 1.0.0
artifacts:
  - file: orc-plugin_teletext_linux_abi20.so
    platform: linux
    abi: 20
    toolchain_tag: gcc15/libstdc++
    sha256: <digest>
```

This repository generates it rather than hand-maintaining it, because the ABI
number and toolchain tag are properties of the compiler that produced each
binary:

- `tools/plugin_build_info.cpp` builds into `orc-plugin-build-info`, compiled
  with the same toolchain and SDK headers as the plugin, and prints the
  descriptor's own `plugin_id`, `plugin_version`, `abi` and `toolchain_tag`.
- `scripts/package_local.sh` reads those values, names and copies the release
  asset, digests it, and writes `dist/plugin-manifest-<platform>.yaml` — a
  complete, valid single-platform manifest.
- `scripts/merge_manifests.sh` combines the per-platform fragments into the
  single `orc-plugin-manifest.yaml` published with the release, failing if the
  fragments disagree about the plugin identity or declare no artifacts.

## CI and Release

- Unified workflow: `.github/workflows/ci.yml`
  - on push/PR: builds, runs the unit and functional test labels, and
    packages on Linux and macOS via Nix, plus a native Windows build; also
    builds the flake's own package (`nix build .#default`) against its pinned
    `decode-orc` revision.
  - on tag push (`v*`): runs the same build/test/package matrix, merges the
    per-platform manifest fragments, then publishes the artifacts and
    `orc-plugin-manifest.yaml` to GitHub Release assets.
- `ORC_SDK_REF` in the workflow selects the decode-orc checkout the SDK comes
  from. It normally tracks `main` (host ABI 20); pin it to a branch or tag only
  for a deliberate, temporary reason, and return it to `main` once that branch
  merges. It is on `issue-377` now: the functional goldens are recorded
  against that branch's VBI Source stage.

## Tagging a Release

```bash
git tag --list        # check for conflicts first
git tag v1.0.0
git push origin v1.0.0
```

This triggers the release workflow, which builds and tests across Linux,
macOS and Windows, packages platform-specific artifacts and their manifest
fragments, merges them into `orc-plugin-manifest.yaml`, and creates a GitHub
Release with the tagged artifacts and the manifest attached.

## License

GPL-3.0-or-later
