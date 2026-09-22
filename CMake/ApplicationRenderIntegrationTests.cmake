# Uses the real Application, Scene, renderer, resources and JobSystem targets.
# Only platform/input/resource/render backends are test doubles.
function(dxf_add_application_render_integration_tests CoreTarget)
    get_filename_component(_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    add_executable(dxf_application_render_integration_tests
        "${_root}/Tests/TestMain.cpp"
        "${_root}/Tests/ApplicationRenderIntegrationTests.cpp")
    target_link_libraries(dxf_application_render_integration_tests PRIVATE ${CoreTarget})
    target_include_directories(dxf_application_render_integration_tests PRIVATE "${_root}/Tests")
    target_compile_features(dxf_application_render_integration_tests PRIVATE cxx_std_20)
    if(COMMAND dxf_warnings)
        dxf_warnings(dxf_application_render_integration_tests)
    endif()
    if(COMMAND dxf_ide_headers)
        dxf_ide_headers(dxf_application_render_integration_tests Tests/Support)
    endif()
    add_test(NAME ApplicationRenderIntegration COMMAND dxf_application_render_integration_tests)
    set_tests_properties(ApplicationRenderIntegration PROPERTIES TIMEOUT 120 LABELS "portable;application;render;threading")
endfunction()
