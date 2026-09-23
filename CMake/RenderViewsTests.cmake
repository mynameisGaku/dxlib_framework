# New dimension/view tests link the real library; only the native boundary is doubled.
function(dxf_add_render_views_tests CoreTarget)
    get_filename_component(_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    add_executable(dxf_render_views_tests
        "${_root}/Tools/RenderValidation/Main.cpp"
        "${_root}/Tests/RenderViewsTests.cpp"
        "${_root}/Tests/DebugDrawStoreTests.cpp")
    target_link_libraries(dxf_render_views_tests PRIVATE ${CoreTarget})
    target_include_directories(dxf_render_views_tests PRIVATE "${_root}/Tests")
    add_executable(dxf_native_views_tests
        "${_root}/Tools/RenderValidation/Main.cpp"
        "${_root}/Tests/NativeRenderViewsTests.cpp"
        "${_root}/Source/Native/Private/Dxf/DxLibRenderBackend.cpp"
        "${_root}/Source/Native/Private/Dxf/DxLibGeometryBackend.cpp"
        "${_root}/Source/Native/Private/Dxf/DxLibModelBackend.cpp")
    target_link_libraries(dxf_native_views_tests PRIVATE ${CoreTarget})
    target_compile_definitions(dxf_native_views_tests PRIVATE DXF_DXLIB_MODELS=1)
    target_include_directories(dxf_native_views_tests PRIVATE "${_root}/Tests" "${_root}/Source/Native/Public"
        "${_root}/Tools/RenderApiValidation/FakeNative" "${_root}/Tests/FakeDxLib")
    foreach(_target dxf_render_views_tests dxf_native_views_tests)
        target_compile_features(${_target} PRIVATE cxx_std_20)
        if(COMMAND dxf_warnings)
            dxf_warnings(${_target})
        elseif(MSVC)
            target_compile_options(${_target} PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /GR)
        else()
            target_compile_options(${_target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
        endif()
        if(COMMAND dxf_ide_headers)
            dxf_ide_headers(${_target} Tests Tools/RenderApiValidation)
        endif()
    endforeach()
    add_test(NAME RenderViews COMMAND dxf_render_views_tests)
    add_test(NAME NativeViewsTranslation COMMAND dxf_native_views_tests)
    set_tests_properties(RenderViews NativeViewsTranslation PROPERTIES TIMEOUT 60 LABELS "portable;render;debug-views")
endfunction()
