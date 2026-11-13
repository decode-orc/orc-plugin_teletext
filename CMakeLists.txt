cmake_minimum_required(VERSION 3.20)

project(orc-plugin-teletext
    VERSION 1.0.0
    DESCRIPTION "Decode-Orc teletext and NABTS stage plugin"
    LANGUAGES CXX
)

set(ORC_PLUGIN_VERSION "${PROJECT_VERSION}" CACHE STRING
    "Plugin version string embedded in StagePluginDescriptor")

# On tagged CI builds, derive the plugin version from tag names like v1.2.3
# unless the caller explicitly overrides ORC_PLUGIN_VERSION.
if(ORC_PLUGIN_VERSION STREQUAL PROJECT_VERSION)
    if(DEFINED ENV{GITHUB_REF_TYPE} AND "$ENV{GITHUB_REF_TYPE}" STREQUAL "tag" AND DEFINED ENV{GITHUB_REF_NAME})
        set(_orc_ref_name "$ENV{GITHUB_REF_NAME}")
        if(_orc_ref_name MATCHES "^v(.+)$")
            set(ORC_PLUGIN_VERSION "${CMAKE_MATCH_1}")
            set(ORC_PLUGIN_VERSION "${ORC_PLUGIN_VERSION}" CACHE STRING
                "Plugin version string embedded in StagePluginDescriptor" FORCE)
        endif()
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build the plugin's unit and functional tests" ON)

# decode-orc turns this on when it builds this repository as a bundled
# submodule (ORC_BUILD_OFFICIAL_PLUGINS), so the host treats the stages as
# core stages. Release assets for the Plugin Manager leave it OFF.
option(ORC_TELETEXT_AS_CORE_PLUGIN
    "Mark the plugin as a core (bundled) plugin in its descriptor" OFF)

# The Nix dev shell exports ORC_INTREE_SDK_DIR (the flake's pinned decode-orc
# source); an explicit -DORC_INTREE_SDK_DIR takes precedence over it.
if(NOT DEFINED ORC_INTREE_SDK_DIR AND DEFINED ENV{ORC_INTREE_SDK_DIR})
    set(ORC_INTREE_SDK_DIR "$ENV{ORC_INTREE_SDK_DIR}")
endif()

# When this project is a subdirectory of a decode-orc build, the host has
# already defined orc-plugin-sdk and orc_add_stage_plugin(); use those.
if(TARGET orc-plugin-sdk)
    set(ORC_TELETEXT_SDK_TARGET orc-plugin-sdk)
else()
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DecodeOrcPluginSDKHelpers.cmake")
    if(DEFINED ORC_INTREE_SDK_DIR)
        # Build the SDK targets (headers + the orc-sdk-support static library)
        # from a decode-orc checkout. No host library is built or linked.
        include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DecodeOrcInTreeSDK.cmake")
        set(ORC_TELETEXT_SDK_TARGET orc-plugin-sdk)
    else()
        find_package(decode-orc-plugin-sdk REQUIRED)
        set(ORC_TELETEXT_SDK_TARGET orc::plugin-sdk)
    endif()
endif()

# --- Shared WST/NABTS decoding library -------------------------------------

add_subdirectory(src/vbi-services)

# --- The plugin: one binary registering teletext_sink and nabts_sink --------

set(ORC_TELETEXT_PLUGIN_TARGET orc-stage-plugin-teletext)

set(ORC_TELETEXT_SINK_SOURCES
    src/teletext_sink/teletext_sink_stage.cpp
    src/teletext_sink/teletext_sink_deps.cpp
    src/teletext_sink/teletext_frame_slicer.cpp
    src/teletext_sink/teletext_scan_state.cpp
    src/teletext_sink/teletext_block_scanner.cpp
    src/teletext_sink/teletext_page_catalogue.cpp
    src/teletext_sink/teletext_squash_stats.cpp
    src/teletext_sink/teletext_packet_spill.cpp
    src/teletext_sink/teletext_catalogue_view.cpp
)

set(ORC_NABTS_SINK_SOURCES
    src/nabts_sink/nabts_sink_stage.cpp
    src/nabts_sink/nabts_sink_deps.cpp
    src/nabts_sink/nabts_frame_slicer.cpp
    src/nabts_sink/nabts_scan_state.cpp
    src/nabts_sink/nabts_block_scanner.cpp
    src/nabts_sink/nabts_packet.cpp
    src/nabts_sink/nabts_data_group.cpp
    src/nabts_sink/nabts_record.cpp
    src/nabts_sink/nabts_record_catalogue.cpp
    src/nabts_sink/naplps_code_env.cpp
    src/nabts_sink/naplps_lint.cpp
    src/nabts_sink/naplps_lint_repair.cpp
    src/nabts_sink/naplps_pdi.cpp
    src/nabts_sink/naplps_state.cpp
    src/nabts_sink/naplps_interpreter.cpp
    src/nabts_sink/naplps_raster.cpp
    src/nabts_sink/naplps_font.cpp
    src/nabts_sink/nabts_raster_view.cpp
    src/nabts_sink/nabts_catalogue_view.cpp
)

# The stage sources as an object library: the plugin and the test binaries
# link the same objects, compiled once.
add_library(orc-teletext-stages OBJECT
    ${ORC_TELETEXT_SINK_SOURCES}
    ${ORC_NABTS_SINK_SOURCES}
)
set_target_properties(orc-teletext-stages PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(orc-teletext-stages PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/teletext_sink
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nabts_sink
)
target_link_libraries(orc-teletext-stages PUBLIC orc-vbi-services ${ORC_TELETEXT_SDK_TARGET})
# Same compile-time log level orc_add_stage_plugin() gives the plugin target.
target_compile_definitions(orc-teletext-stages PRIVATE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG>
    $<$<NOT:$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE>
)

orc_add_stage_plugin(
    ${ORC_TELETEXT_PLUGIN_TARGET}
    OUTPUT_NAME ${ORC_TELETEXT_PLUGIN_TARGET}
    PLUGIN_VERSION ${ORC_PLUGIN_VERSION}
    SOURCES
        src/plugin.cpp
    # Linking an OBJECT library links its objects into the plugin.
    LINK_LIBRARIES orc-teletext-stages
)

if(ORC_TELETEXT_AS_CORE_PLUGIN)
    target_compile_definitions(${ORC_TELETEXT_PLUGIN_TARGET} PRIVATE ORC_TELETEXT_CORE_PLUGIN=1)
endif()

# --- Per-stage instructions -------------------------------------------------
#
# One library registers two stages, so each stage reads
# <library base name>.<stage name>.md (src/common/stage_instructions_file.h)
# rather than the single <library base name>.md the SDK macro reads.

if(NOT DEFINED ORC_STAGE_PLUGIN_BUILD_DIR)
    set(ORC_STAGE_PLUGIN_BUILD_DIR "${CMAKE_BINARY_DIR}/lib/orc-stage-plugins")
endif()
if(WIN32)
    set(_orc_teletext_md_base "${ORC_TELETEXT_PLUGIN_TARGET}")
else()
    set(_orc_teletext_md_base "lib${ORC_TELETEXT_PLUGIN_TARGET}")
endif()

set(_orc_teletext_md_outputs)
foreach(_stage teletext_sink nabts_sink)
    set(_src "${CMAKE_CURRENT_SOURCE_DIR}/src/${_stage}/instructions.md")
    set(_name "${_orc_teletext_md_base}.${_stage}.md")
    set(_dst "${ORC_STAGE_PLUGIN_BUILD_DIR}/${_name}")
    add_custom_command(
        OUTPUT "${_dst}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_src}" "${_dst}"
        DEPENDS "${_src}"
        COMMENT "Copying ${_stage} instructions.md"
        VERBATIM
    )
    list(APPEND _orc_teletext_md_outputs "${_dst}")
    install(FILES "${_src}"
        RENAME "${_name}"
        DESTINATION "${ORC_STAGE_PLUGIN_INSTALL_DIR}"
        COMPONENT Runtime
    )
endforeach()
add_custom_target(${ORC_TELETEXT_PLUGIN_TARGET}-instructions ALL
    DEPENDS ${_orc_teletext_md_outputs})
add_dependencies(${ORC_TELETEXT_PLUGIN_TARGET} ${ORC_TELETEXT_PLUGIN_TARGET}-instructions)

# --- Release-manifest build facts -------------------------------------------
#
# Compiled with the same toolchain and SDK headers as the plugin, it prints the
# descriptor's ABI version and toolchain tag so scripts/package_local.sh can
# declare them in orc-plugin-manifest.yaml without guessing. Not needed when
# decode-orc builds this repository as a submodule.

if(NOT ORC_TELETEXT_AS_CORE_PLUGIN)
    add_executable(orc-plugin-build-info tools/plugin_build_info.cpp)
    target_link_libraries(orc-plugin-build-info PRIVATE ${ORC_TELETEXT_SDK_TARGET})
    target_include_directories(orc-plugin-build-info PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(orc-plugin-build-info PRIVATE
        ORC_STAGE_PLUGIN_VERSION="${ORC_PLUGIN_VERSION}")
    set_target_properties(orc-plugin-build-info PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
