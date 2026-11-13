# Tests for the teletext/NABTS plugin.
#
#   unit/        ctest label "unit": stage classes and the vbi-services
#                library, driven in-process against SDK-only mocks.
#   functional/  ctest label "functional": the stages run end to end on real
#                captures read through decode-orc's VBI Source stage.
#   view/        ctest label "view": the view binary's widgets, offscreen.

find_package(GTest REQUIRED)
include(GoogleTest)

add_subdirectory(unit)
add_subdirectory(functional)
if(BUILD_VIEW)
    add_subdirectory(view)
endif()
