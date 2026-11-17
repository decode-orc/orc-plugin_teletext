# DecodeOrcInTreeSDK.cmake
#
# Builds the decode-orc plugin SDK targets straight from a decode-orc source
# checkout (ORC_INTREE_SDK_DIR), for CI and local work without an installed
# SDK package. It defines the same two targets the installed package exports,
# under their in-tree names:
#
#   orc-sdk-support  STATIC library of the support-tier helpers
#                    (<orc/support/logging.h>, scratch_file.h,
#                    preview_helpers.h, observers/, ...), compiled from
#                    every .cpp under <checkout>/orc/sdk/src.
#   orc-plugin-sdk   INTERFACE target: the SDK include directory and
#                    orc-sdk-support.
#
# Both are built with private logging (orc_sdk_enable_private_logging() in
# DecodeOrcPluginSDKHelpers.cmake, which must be included first): spdlog and
# fmt are compiled in header-only, so the plugin links no spdlog or fmt
# library and loads whatever versions orc-gui was built with (decode-orc issue
# #391). Its ORC_LOG_* records reach the host through OrcPluginServices::log.
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

# A Windows path given on the command line keeps its backslashes, and the
# CONFIGURE_DEPENDS glob below writes it verbatim into VerifyGlobs.cmake,
# where "\a" in "D:\a\..." is read as an escape and the build stops.
file(TO_CMAKE_PATH "${ORC_INTREE_SDK_DIR}" ORC_INTREE_SDK_DIR)
set(_orc_sdk_root "${ORC_INTREE_SDK_DIR}/orc/sdk")
if(NOT EXISTS "${_orc_sdk_root}/include/orc/abi/orc_plugin_abi.h")
    message(FATAL_ERROR
        "ORC_INTREE_SDK_DIR (${ORC_INTREE_SDK_DIR}) is not a decode-orc checkout: "
        "${_orc_sdk_root}/include/orc/abi/orc_plugin_abi.h is missing")
endif()

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
if(NOT COMMAND orc_sdk_enable_private_logging)
    message(FATAL_ERROR
        "DecodeOrcInTreeSDK.cmake needs DecodeOrcPluginSDKHelpers.cmake included first")
endif()
orc_sdk_enable_private_logging()

# Recursive: the observers live in src/observers/.
file(GLOB_RECURSE _orc_sdk_support_sources CONFIGURE_DEPENDS "${_orc_sdk_root}/src/*.cpp")
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
target_link_libraries(orc-sdk-support PUBLIC orc-sdk-private-logging)
# decode-orc compiles everything with these on Windows, and the SDK headers
# rely on them: NOMINMAX keeps windows.h's min()/max() macros from breaking
# std::min/std::max and std::numeric_limits<T>::max(), and _USE_MATH_DEFINES
# exposes M_PI. PUBLIC, so the plugin and its tests get them too.
if(WIN32)
    target_compile_definitions(orc-sdk-support PUBLIC NOMINMAX _USE_MATH_DEFINES)
endif()

add_library(orc-plugin-sdk INTERFACE)
target_include_directories(orc-plugin-sdk INTERFACE "${_orc_sdk_root}/include")
# Bundled plugins build without the pre-tier include shims; do the same so a
# plugin that still uses a pre-tier path fails here rather than in decode-orc.
target_compile_definitions(orc-plugin-sdk INTERFACE ORC_SDK_NO_DEPRECATED_INCLUDE_SHIMS)
target_link_libraries(orc-plugin-sdk INTERFACE orc-sdk-support)
