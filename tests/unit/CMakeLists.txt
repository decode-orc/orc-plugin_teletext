# Unit tests, grouped into the same three binaries decode-orc built them in.

# DAGStage::execute() takes a concrete orc::ObservationContext, which the host
# defines; the tests build their own copy (support/observation_context.cpp).
add_library(orc-teletext-test-support STATIC support/observation_context.cpp)
target_link_libraries(orc-teletext-test-support PUBLIC ${ORC_TELETEXT_SDK_TARGET})

function(orc_teletext_add_unit_tests target)
    add_executable(${target} ${ARGN})
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(${target} PRIVATE
        orc-teletext-stages
        orc-teletext-test-support
        GTest::gmock_main
    )
    gtest_discover_tests(${target}
        DISCOVERY_MODE POST_BUILD
        DISCOVERY_TIMEOUT 30
        PROPERTIES LABELS "unit")
endfunction()

orc_teletext_add_unit_tests(orc-teletext-unit-tests-nabts-sink
    stages/nabts_sink/nabts_sink_stage_test.cpp
    stages/nabts_sink/nabts_frame_slicer_test.cpp
    stages/nabts_sink/nabts_packet_test.cpp
    stages/nabts_sink/nabts_data_group_test.cpp
    stages/nabts_sink/nabts_salvage_test.cpp
    stages/nabts_sink/nabts_datacast_files_test.cpp
    stages/nabts_sink/nabts_framed_service_test.cpp
    stages/nabts_sink/nabts_web_mirror_test.cpp
    stages/nabts_sink/nabts_framed_containers_test.cpp
    stages/nabts_sink/nabts_framed_catalogue_test.cpp
    stages/nabts_sink/nabts_decompress_test.cpp
    stages/nabts_sink/gif_decoder_test.cpp
    stages/nabts_sink/nabts_datacast_test.cpp
    stages/nabts_sink/nabts_service_census_test.cpp
    stages/nabts_sink/nabts_network_messages_test.cpp
    stages/nabts_sink/nabts_data_export_test.cpp
    stages/nabts_sink/nabts_playlist_test.cpp
    stages/nabts_sink/nabts_record_test.cpp
    stages/nabts_sink/nabts_record_catalogue_test.cpp
    stages/nabts_sink/nabts_navigation_test.cpp
    stages/nabts_sink/nabts_caption_publication_test.cpp
    stages/nabts_sink/nabts_reference_records_test.cpp
    stages/nabts_sink/nabts_caption_stream_test.cpp
    stages/nabts_sink/nabts_catalogue_view_test.cpp
    stages/nabts_sink/naplps_code_env_test.cpp
    stages/nabts_sink/naplps_font_test.cpp
    stages/nabts_sink/naplps_lint_test.cpp
    stages/nabts_sink/naplps_lint_repair_test.cpp
    stages/nabts_sink/naplps_interpreter_test.cpp
    stages/nabts_sink/naplps_coordinate_test.cpp
    stages/nabts_sink/naplps_palette_timeline_test.cpp
    stages/nabts_sink/naplps_receiver_pace_test.cpp
    stages/nabts_sink/naplps_listing_test.cpp
    stages/nabts_sink/naplps_proportional_test.cpp
    stages/nabts_sink/nabts_record_library_test.cpp
    stages/nabts_sink/naplps_dialect_model_test.cpp
    stages/nabts_sink/naplps_reconstruct_test.cpp
    stages/nabts_sink/naplps_raster_test.cpp
    stages/nabts_sink/naplps_render_grid_test.cpp
    stages/nabts_sink/nabts_scan_state_test.cpp
    stages/nabts_sink/nabts_block_scanner_test.cpp
    stages/nabts_sink/nabts_sink_deps_test.cpp
)

orc_teletext_add_unit_tests(orc-teletext-unit-tests-teletext-sink
    stages/teletext_sink/teletext_sink_stage_test.cpp
    stages/teletext_sink/teletext_sink_deps_test.cpp
    stages/teletext_sink/teletext_frame_slicer_test.cpp
    stages/teletext_sink/teletext_scan_state_test.cpp
    stages/teletext_sink/teletext_block_scanner_test.cpp
    stages/teletext_sink/teletext_page_catalogue_test.cpp
    stages/teletext_sink/teletext_catalogue_view_test.cpp
    stages/teletext_sink/teletext_squash_stats_test.cpp
    stages/teletext_sink/teletext_unplaced_rows_test.cpp
    stages/teletext_sink/teletext_data_channels_test.cpp
    stages/teletext_sink/teletext_page_format_clear_test.cpp
    stages/teletext_sink/teletext_data_export_test.cpp
)

orc_teletext_add_unit_tests(orc-teletext-unit-tests-vbi-services
    common/vbi-services/nabts_page_test.cpp
    common/vbi-services/nabts_repertoire_test.cpp
    common/vbi-services/teletext_bit_clock_test.cpp
    common/vbi-services/teletext_slicer_test.cpp
    common/vbi-services/teletext_page_decoder_test.cpp
    common/vbi-services/teletext_recovery_stats_test.cpp
    common/vbi-services/teletext_row_squasher_test.cpp
    common/vbi-services/vbi_identity_attestation_test.cpp
    common/vbi-services/vbi_export_writer_test.cpp
)
