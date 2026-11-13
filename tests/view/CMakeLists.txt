# GUI tests for the view binary's widgets, run offscreen (each test makes its
# own QApplication on the offscreen platform). ctest label "view".
#
# The tests use Qt Test (QSignalSpy); the view binary itself does not.
find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(orc-teletext-view-tests
    catalogue_dialog_test.cpp
    catalogue_display_list_reveal_test.cpp
    catalogue_display_list_vector_test.cpp
    catalogue_playback_test.cpp
    catalogue_presentation_timeline_test.cpp
    catalogue_session_controller_test.cpp
    catalogue_stroke_geometry_test.cpp
    catalogue_svg_document_test.cpp
    teletext_font_test.cpp
)
target_link_libraries(orc-teletext-view-tests PRIVATE
    orc-teletext-view-widgets
    Qt6::Test
    GTest::gtest_main
)
gtest_discover_tests(orc-teletext-view-tests
    DISCOVERY_MODE POST_BUILD
    DISCOVERY_TIMEOUT 60
    PROPERTIES
        LABELS "view"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
