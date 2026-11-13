# Tests for the teletext/NABTS plugin.
#
#   unit/        ctest label "unit": stage classes and the vbi-services
#                library, driven in-process against SDK-only mocks.
#   functional/  ctest label "functional": the stages run end to end on real
#                captures read through decode-orc's VBI Source stage.
#   view/        GUI tests for the catalogue viewer; not built until the
#                viewer is a view plugin.

find_package(GTest REQUIRED)
include(GoogleTest)

add_subdirectory(unit)
add_subdirectory(functional)
