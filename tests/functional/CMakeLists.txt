# Functional tests (ctest label "functional"): the stages run end to end on
# real WST and NABTS captures, and the exported streams are compared against
# golden digests.
#
# The captures are read through decode-orc's VBI Source stage, which stays in
# decode-orc. Its sources compile against the public plugin SDK plus FFmpeg
# (which unwraps FLAC-compressed captures), so they are built here, from the
# same decode-orc checkout the SDK comes from, into a test-only static
# library. Nothing from it reaches the plugin binary.
#
# The captures are not checked in. Point ORC_TELETEXT_TEST_DATA_DIR at a
# directory laid out like decode-orc's test-data/ (teletext/...); each test
# skips when its capture is absent.

add_executable(orc-teletext-functional-sha256-test
    support/sha256_hash.cpp
    support/sha256_hash_test.cpp
)
target_link_libraries(orc-teletext-functional-sha256-test PRIVATE
    GTest::gmock_main
)
gtest_discover_tests(orc-teletext-functional-sha256-test
    DISCOVERY_MODE POST_BUILD
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS "functional")

set(ORC_TELETEXT_TEST_DATA_DIR "${PROJECT_SOURCE_DIR}/test-data" CACHE PATH
    "Directory holding the reference VBI captures (laid out like decode-orc's test-data/)")

# Where the VBI Source stage sources come from: the decode-orc checkout the
# SDK is built from, or the host tree when decode-orc builds this repository
# as a submodule.
if(DEFINED ORC_INTREE_SDK_DIR)
    set(_orc_vbi_source_dir "${ORC_INTREE_SDK_DIR}/orc/plugins/stages/vbi_source")
elseif(DEFINED decode-orc-root_SOURCE_DIR)
    set(_orc_vbi_source_dir "${decode-orc-root_SOURCE_DIR}/orc/plugins/stages/vbi_source")
endif()

if(NOT _orc_vbi_source_dir OR NOT EXISTS "${_orc_vbi_source_dir}/vbi_source_stage.h")
    message(STATUS "Functional pipeline tests disabled: no decode-orc checkout "
                   "with orc/plugins/stages/vbi_source (set ORC_INTREE_SDK_DIR)")
    return()
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ORC_TELETEXT_FFMPEG IMPORTED_TARGET libavformat libavcodec libavutil)
endif()
if(NOT ORC_TELETEXT_FFMPEG_FOUND)
    message(STATUS "Functional pipeline tests disabled: FFmpeg (libavformat, "
                   "libavcodec, libavutil) not found; the VBI Source stage needs it")
    return()
endif()

# Every .cpp except plugin.cpp: the stage class and its library, without the
# plugin entrypoints (the tests drive the class in-process).
file(GLOB _orc_vbi_source_sources CONFIGURE_DEPENDS "${_orc_vbi_source_dir}/*.cpp")
list(FILTER _orc_vbi_source_sources EXCLUDE REGEX "/plugin\\.cpp$")

add_library(orc-teletext-test-vbi-source STATIC ${_orc_vbi_source_sources})
target_include_directories(orc-teletext-test-vbi-source PUBLIC "${_orc_vbi_source_dir}")
target_link_libraries(orc-teletext-test-vbi-source PUBLIC
    ${ORC_TELETEXT_SDK_TARGET}
    PkgConfig::ORC_TELETEXT_FFMPEG
)

function(orc_teletext_add_pipeline_test target source)
    add_executable(${target} ${source} support/sha256_hash.cpp)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
    target_link_libraries(${target} PRIVATE
        orc-teletext-stages
        orc-teletext-test-vbi-source
        orc-teletext-test-support
        GTest::gmock_main
    )
    target_compile_definitions(${target} PRIVATE
        ORC_VBI_TEST_DATA_DIR="${ORC_TELETEXT_TEST_DATA_DIR}")
    gtest_discover_tests(${target}
        DISCOVERY_MODE POST_BUILD
        DISCOVERY_TIMEOUT 30
        PROPERTIES LABELS "functional")
endfunction()

orc_teletext_add_pipeline_test(orc-teletext-functional-tests-teletext-sink
    stages/teletext_sink/teletext_sink_pipeline_test.cpp)
orc_teletext_add_pipeline_test(orc-teletext-functional-tests-nabts-sink
    stages/nabts_sink/nabts_sink_pipeline_test.cpp)
