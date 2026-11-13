# The plugin's view binary, orc-view_teletext: the catalogue browser that the
# Teletext Pages and NABTS Records tools open in orc-gui.
#
# Only Qt Core, Gui and Widgets: the modules every orc-gui ships. Build it
# against the view Qt baseline (decode-orc's plugin-sdk.md, "View Qt
# baseline"); orc-gui refuses a view built for a newer Qt minor than its own.
#
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns

# The widgets as a static library, so the view binary and its tests compile
# them once. Hidden visibility: orc-gui has classes of the same names while
# decode-orc still carries its own catalogue browser, and nothing here may
# bind to them (or they to it).
add_library(orc-teletext-view-widgets STATIC
    cataloguedialog.cpp
    cataloguecellgridwidget.cpp
    cataloguedisplaylistwidget.cpp
    catalogue_animation_clock.cpp
    catalogue_display_list_canvas.cpp
    catalogue_flash_clock.cpp
    catalogue_keypad.cpp
    catalogue_playback.cpp
    catalogue_playback_bar.cpp
    catalogue_presentation_timeline.cpp
    catalogue_stroke_geometry.cpp
    catalogue_svg_document.cpp
    catalogue_session_controller.cpp
    teletext_font.cpp
    teletext_glyph_painter.cpp
)
set_target_properties(orc-teletext-view-widgets PROPERTIES
    AUTOMOC ON
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
target_include_directories(orc-teletext-view-widgets PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})
# The SDK target for the <orc/view/...> headers. The view uses nothing else of
# it: no stage header is included here (scripts/check_sdk_boundary.sh).
target_link_libraries(orc-teletext-view-widgets PUBLIC
    Qt6::Widgets orc-teletext-catalogue ${ORC_TELETEXT_SDK_TARGET})

add_library(${ORC_TELETEXT_VIEW_TARGET} SHARED teletext_view_plugin.cpp)
target_link_libraries(${ORC_TELETEXT_VIEW_TARGET} PRIVATE
    orc-teletext-view-widgets)
target_compile_definitions(${ORC_TELETEXT_VIEW_TARGET} PRIVATE
    ORC_VIEW_PLUGIN_VERSION="${ORC_PLUGIN_VERSION}")
get_target_property(_orc_teletext_plugin_dir ${ORC_TELETEXT_PLUGIN_TARGET}
    LIBRARY_OUTPUT_DIRECTORY)
# No "lib" prefix: hosts tell a view binary apart by its orc-view_ name.
# Beside the stage binary, so one ORC_STAGE_PLUGIN_PATHS folder gives orc-gui
# both halves.
set_target_properties(${ORC_TELETEXT_VIEW_TARGET} PROPERTIES
    PREFIX ""
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    LIBRARY_OUTPUT_DIRECTORY "${_orc_teletext_plugin_dir}"
    RUNTIME_OUTPUT_DIRECTORY "${_orc_teletext_plugin_dir}")
install(TARGETS ${ORC_TELETEXT_VIEW_TARGET}
    LIBRARY DESTINATION "${ORC_STAGE_PLUGIN_INSTALL_DIR}" COMPONENT Runtime
    RUNTIME DESTINATION "${ORC_STAGE_PLUGIN_INSTALL_DIR}" COMPONENT Runtime)
