# Real planner/renderer/native adapters; only SDK calls and device output are doubled.
function(dxf_add_render_transparency_tests CoreTarget)
    get_filename_component(_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    add_executable(dxf_transparency_tests "${_root}/Tools/RenderValidation/Main.cpp"
        "${_root}/Tests/RenderTransparencyTests.cpp"
        "${_root}/Examples/RenderDebug/TransparencyDemo.cpp")
    add_executable(dxf_transparency_fault_tests "${_root}/Tools/RenderValidation/TransparencyFaultTests.cpp"
        "${_root}/Source/Toolbox/Private/Toolbox/Testing/AllocationFault.cpp")
    target_compile_definitions(dxf_transparency_fault_tests PRIVATE DXF_ALLOCATION_FAULT_TEST_EXECUTABLE=1)
    add_executable(dxf_native_transparency_tests "${_root}/Tools/RenderValidation/Main.cpp"
        "${_root}/Tests/NativeTransparencyTests.cpp"
        "${_root}/Source/Native/Private/Dxf/DxLibRenderBackend.cpp"
        "${_root}/Source/Native/Private/Dxf/DxLibGeometryBackend.cpp")
    foreach(_target dxf_transparency_tests dxf_transparency_fault_tests dxf_native_transparency_tests)
        target_link_libraries(${_target} PRIVATE ${CoreTarget})
        target_compile_features(${_target} PRIVATE cxx_std_20)
        target_include_directories(${_target} PRIVATE "${_root}/Tests")
        if(COMMAND dxf_warnings)
            dxf_warnings(${_target})
        endif()
    endforeach()
    target_include_directories(dxf_native_transparency_tests PRIVATE "${_root}/Source/Native/Public"
        "${_root}/Tools/RenderApiValidation/FakeNative" "${_root}/Tests/FakeDxLib")
    add_test(NAME RenderTransparency COMMAND dxf_transparency_tests)
    add_test(NAME RenderTransparencyFault COMMAND dxf_transparency_fault_tests)
    add_test(NAME NativeTransparency COMMAND dxf_native_transparency_tests)
    set_tests_properties(RenderTransparency RenderTransparencyFault NativeTransparency
        PROPERTIES TIMEOUT 60 LABELS "portable;render;transparency")
endfunction()
