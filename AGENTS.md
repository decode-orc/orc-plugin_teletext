# AI Instructions for orc-plugin_teletext

Use these instructions for all changes in this repository.

## What this repository is
This is a Decode-Orc external stage plugin, not part of decode-orc core. It
builds one shared library (`org.decodeorc.stage.teletext`) that registers two
stages, `teletext_sink` (World System Teletext, ITU-R BT.653 System B) and
`nabts_sink` (North American Basic Teletext, System C), plus the
`orc-vbi-services` static library they share (WST/NABTS line slicing, page and
record decoding). Both stages moved here from decode-orc core (issue #377);
`git log --follow` on a source file reaches its decode-orc history.

## Source of Truth
- For SDK usage and ABI contracts, use only public SDK headers under the
  tiered SDK layout:
  - `orc/sdk/include/orc/abi/` (frozen binary contract: descriptor,
    entrypoints, registration, services — include `<orc/abi/orc_plugin_sdk.h>`)
  - `orc/sdk/include/orc/plugin/` (stage API/runtime/preview aggregators)
  - `orc/sdk/include/orc/stage/` (stage interfaces and data-contract types)
  - `orc/sdk/include/orc/support/` (header-only/helper conveniences)
- Follow current decode-orc patterns from main for anything not covered here:
  https://github.com/decode-orc/decode-orc/tree/main/orc/plugins

## SDK Boundary (Hard Rule)
- This repository is an external plugin project.
- Only use the provided plugin SDK surface.
- Do not include private host/core headers from decode-orc internals.
- Do not link against host internal libraries or targets (for example
  `orc-core`, `orc-common`, `orc-metadata`, presenter/gui/cli targets).
- Do not call non-SDK helper APIs that are not part of the plugin SDK contract.
- `scripts/check_sdk_boundary.sh` enforces this; CI runs it, and so should you
  before finalizing a change.

## Plugin Structure Rules
- `src/vbi-services/` is shared between both stages but is NOT part of the SDK
  contract — it is plugin-local code, not a third-party API. See the comment
  at the top of `src/vbi-services/CMakeLists.txt` for why it left the SDK.
- `src/plugin.cpp`/`src/plugin.h` hold the one descriptor and the
  `ORC_DEFINE_STAGE_PLUGIN` call that registers both stage classes. Do not add
  a second plugin binary for a single additional stage; register it from the
  same entrypoint instead, unless it genuinely does not belong under
  `org.decodeorc.stage.teletext`.
- Because one library registers two stages, `ORC_STAGE_INSTRUCTIONS_MD` (which
  reads `<library>.md`) cannot tell them apart. Both stage classes use
  `ORC_TELETEXT_STAGE_INSTRUCTIONS_MD(stage_name)` from
  `src/common/stage_instructions_file.h` instead, which reads
  `<library>.<stage_name>.md`. `CMakeLists.txt` copies each stage's
  `instructions.md` to that name at build time and install time.
- `ORC_TELETEXT_AS_CORE_PLUGIN` (CMake option, default `OFF`) sets the
  descriptor's `is_core_plugin` flag. decode-orc turns it `ON` when it builds
  this repository as a bundled submodule (`external/plugins/`); leave it `OFF`
  for release builds — the Plugin Manager's trust prompt depends on it being
  correct.
- Do not declare a menu/sink category. `NodeTypeInfo` has no `menu_category`
  or `sink_category`; the Add Stage group is derived from `NodeType` by
  `orc::stage_category_for()` and read back via `NodeTypeInfo::category()`.
- A stage that wants to hand the host a browsable set of items implements
  `ICatalogueResults` from `<orc/stage/tooling/catalogue_results.h>` and
  declares `StageToolKind::CatalogueBrowser` — both stages already do this for
  their page/record catalogues.

## Test Rules
- `tests/unit/` (ctest label `unit`): stage classes and `orc-vbi-services`
  driven in-process against SDK-only mocks (`tests/unit/include/`,
  `tests/unit/stage_services_mock.h`). These must never need a decode-orc
  checkout, a real capture file, or network access.
  - `tests/unit/support/observation_context.cpp` is a test-only definition of
    `orc::ObservationContext` (declared in the SDK, defined by the decode-orc
    host, which a plugin never links). Keep it a verbatim copy of
    `orc/core/observation_context.cpp` in decode-orc; update it only if that
    file's private members change, and say so in the commit.
- `tests/functional/` (ctest label `functional`): the stages run end to end on
  real captures, read through decode-orc's VBI Source stage. That stage stays
  in decode-orc; `tests/functional/CMakeLists.txt` compiles its sources (SDK +
  FFmpeg only) from the decode-orc checkout into a test-only library. Never
  link it into the plugin binary. The captures are not checked in
  (`ORC_TELETEXT_TEST_DATA_DIR`); tests `GTEST_SKIP()` without them. Never add
  a decode-orc `orc/core` header to these tests: `support/sha256_hash.*`
  replaces the host's private one.
- `tests/view/` covers the catalogue viewer GUI; it is not built until the
  viewer becomes a view plugin (tracked separately).

## Release Manifest Rules
- Every release must publish `orc-plugin-manifest.yaml`. A release without a
  valid one cannot be browsed, installed, or updated to.
- Never hand-write the ABI number or toolchain tag into a script or workflow.
  They are properties of the compiler that built the binary: take them from
  `orc-plugin-build-info`, which is compiled with the plugin's own toolchain
  and SDK headers and prints the descriptor's values.
- Keep manifest generation in `scripts/package_local.sh` (per-platform
  fragment, including the SHA-256 of the packaged asset) and
  `scripts/merge_manifests.sh` (fragments to release manifest). Both must stay
  runnable under `bash` on all three matrix platforms.
- Manifest `file` entries must match the release-asset naming convention
  `orc-plugin_teletext_<platform>[_abi<N>].<ext>` and name assets that are
  actually published, or host-side resolution fails. The asset token is the
  plugin's name (`teletext`), not a stage name, because the binary registers
  two stages.

## CMake and Build Rules
- Build the plugin target through `orc_add_stage_plugin()` only.
- For in-tree SDK use, `cmake/DecodeOrcInTreeSDK.cmake` builds the SDK targets
  (headers plus the `orc-sdk-support` static library) from a decode-orc
  checkout; do not depend on internal compiled host libraries.
- For installed SDK use, rely on `find_package(decode-orc-plugin-sdk REQUIRED)`.
- When this repository is added as a decode-orc submodule
  (`external/plugins/orc-plugin_teletext`), decode-orc's own build already
  defines `orc-plugin-sdk` and `orc_add_stage_plugin()`; `CMakeLists.txt`
  detects that (`if(TARGET orc-plugin-sdk)`) and skips its own SDK setup. Keep
  that detection working.
- Never add direct linkage to internal decode-orc targets just to satisfy
  convenience helpers.

## Nix Flake Rules
- `flake.nix` pins `nixpkgs` to the same revision as decode-orc's own flake
  (check `flake.lock` there) and the `decode-orc` input to a specific commit,
  used only for its SDK sources (`orc/sdk`) — never imported as a flake. Move
  either pin only for a deliberate reason, together: a different nixpkgs
  generation gives a different default compiler, and the host rejects a
  plugin whose toolchain tag does not match its own exactly.
- `nix build .#default` builds the plugin and runs the unit tests (not the
  functional tests, which need the reference captures) as its check phase.

## CI/CD Safety Rules
- Keep workflow SDK checkout pointed at decode-orc `main` by default.
- Do not pin `ORC_SDK_REF` to ephemeral or unknown refs.
- If pinning is required, use an existing branch/tag/commit and update
  references in the workflow intentionally, and return it to `main` once that
  branch merges.
- Preserve cross-platform matrix behavior (Linux/macOS/Windows) and packaging
  outputs.
- Preserve artifact names and expected output paths unless a coordinated CI
  change is made.
- Release assets and the release manifest are published together; a change to
  one must update the other.

## Required Validation Before Finalizing Changes
- Configure and build with the in-tree SDK:
  - `cmake -S . -B build -DORC_INTREE_SDK_DIR=<decode-orc checkout> -DBUILD_TESTS=ON`
  - `cmake --build build --parallel`
  - `ctest --test-dir build --output-on-failure -L unit`
  - `ctest --test-dir build --output-on-failure -L functional` (configure with
    `-DORC_TELETEXT_TEST_DATA_DIR=<captures>` to run rather than skip them)
- `./scripts/check_sdk_boundary.sh .`
- Ensure workflow changes remain compatible with all matrix OS entries.

## Change Discipline
- Keep changes minimal and scoped.
- Do not introduce API/ABI assumptions that are not explicitly exposed by the
  SDK.
- If a desired optimization requires host-internal APIs, do not implement it
  here; keep the SDK-compliant approach.
- Do not add a `Co-Authored-By` trailer or a "Generated with" footer to
  commits or pull requests from this repository.
