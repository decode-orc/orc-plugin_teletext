# DecodeOrcInTreeSDK.cmake
#
# Builds the decode-orc plugin SDK targets straight from a decode-orc source
# checkout (ORC_INTREE_SDK_DIR), for CI and local work without an installed
# SDK package. It defines the same two targets the installed package exports,
# under their in-tree names:
#
#   orc-sdk-support  STATIC library of the support-tier helpers
#                    (<orc/support/logging.h>, scratch_file.h,
#                    preview_helpers.h, ...), compiled from
#                    <checkout>/orc/sdk/src/*.cpp.
#   orc-plugin-sdk   INTERFACE target: the SDK include directory, spdlog/fmt,
#                    and orc-sdk-support.
#
# Only the SDK tree is used. No decode-orc host library is built or linked,
# so a plugin built this way has the same link surface as one built against
# find_package(decode-orc-plugin-sdk).
#
# Keep this file in step with orc/sdk/CMakeLists.txt in decode-orc. The
# source list is globbed, so a new support source needs no change here.

include_guard(GLOBAL)

if(NOT DEFINED ORC_INTREE_SDK_DIR)
    message(FATAL_ERROR "DecodeOrcInTreeSDK.cmake needs ORC_INTREE_SDK_DIR")
endif()

set(_orc_sdk_root "${ORC_INTREE_SDK_DIR}/orc/sdk")
if(NOT EXISTS "${_orc_sdk_root}/include/orc/abi/orc_plugin_abi.h")
    message(FATAL_ERROR
        "ORC_INTREE_SDK_DIR (${ORC_INTREE_SDK_DIR}) is not a decode-orc checkout: "
        "${_orc_sdk_root}/include/orc/abi/orc_plugin_abi.h is missing")
endif()

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

file(GLOB _orc_sdk_support_sources CONFIGURE_DEPENDS "${_orc_sdk_root}/src/*.cpp")
if(NOT _orc_sdk_support_sources)
    message(FATAL_ERROR "No SDK support sources found under ${_orc_sdk_root}/src")
endif()

add_library(orc-sdk-support STATIC ${_orc_sdk_support_sources})
set_target_properties(orc-sdk-support PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(orc-sdk-support PUBLIC "${_orc_sdk_root}/include")
# Same compile-time log level as decode-orc builds the library with.
target_compile_definitions(orc-sdk-support PRIVATE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG>
    $<$<NOT:$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE>
)
target_link_libraries(orc-sdk-support PUBLIC spdlog::spdlog fmt::fmt)

add_library(orc-plugin-sdk INTERFACE)
target_include_directories(orc-plugin-sdk INTERFACE "${_orc_sdk_root}/include")
# Bundled plugins build without the pre-tier include shims; do the same so a
# plugin that still uses a pre-tier path fails here rather than in decode-orc.
target_compile_definitions(orc-plugin-sdk INTERFACE ORC_SDK_NO_DEPRECATED_INCLUDE_SHIMS)
target_link_libraries(orc-plugin-sdk INTERFACE orc-sdk-support spdlog::spdlog fmt::fmt)
