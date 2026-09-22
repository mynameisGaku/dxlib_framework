# Optional observations only: Physics does not depend on Debug or rendering.
# Debug reads the value snapshots that Physics captures (FPhysicsWorld2D/3D::CaptureSnapshot).
add_library(dxf_debug_tools STATIC
    Source/Debug/Private/Dxf/DebugCamera3D.cpp
    Source/Debug/Private/Dxf/DebugStepController.cpp
    Source/Debug/Private/Dxf/DebugSnapshotHistory.cpp
    Source/Debug/Private/Dxf/PhysicsDebugSnapshot3D.cpp
    Source/Debug/Private/Dxf/PhysicsDebugSnapshot2D.cpp
    Source/Debug/Private/Dxf/PhysicsDebugRecorder3D.cpp
    Source/Debug/Private/Dxf/PhysicsDebugDisplay3D.cpp
    Source/Debug/Private/Dxf/PhysicsDebugDisplay2D.cpp)
add_library(dxf::debug_tools ALIAS dxf_debug_tools)
set_target_properties(dxf_debug_tools PROPERTIES EXPORT_NAME debug_tools DEBUG_POSTFIX d)
target_compile_features(dxf_debug_tools PUBLIC cxx_std_20)
target_link_libraries(dxf_debug_tools PUBLIC dxf::support dxf::physics)
dxf_public_headers(dxf_debug_tools Debug PUBLIC)
dxf_warnings(dxf_debug_tools)
list(APPEND DXF_EXPORT_TARGETS dxf_debug_tools)
if(DXF_INSTALL)
    install(DIRECTORY "Source/Debug/Public/" DESTINATION include FILES_MATCHING PATTERN "*.h")
endif()
if(DXF_BUILD_TESTS)
    add_executable(dxf_debug_tools_tests Tools/RenderValidation/Main.cpp
        Tests/DebugToolsTests.cpp Tests/DebugRendererIntegrationTests.cpp)
    target_link_libraries(dxf_debug_tools_tests PRIVATE dxf::debug_tools)
    target_include_directories(dxf_debug_tools_tests PRIVATE Tests)
    dxf_ide_headers(dxf_debug_tools_tests Tests/Support)
    dxf_warnings(dxf_debug_tools_tests)
    add_test(NAME DebugTools COMMAND dxf_debug_tools_tests)
    set_tests_properties(DebugTools PROPERTIES TIMEOUT 60 LABELS "portable;debug-tools")
    add_executable(dxf_debug_physics_tests Tools/RenderValidation/Main.cpp Tests/DebugPhysicsIntegrationTests.cpp)
    target_link_libraries(dxf_debug_physics_tests PRIVATE dxf::debug_tools dxf::physics)
    target_include_directories(dxf_debug_physics_tests PRIVATE Tests)
    dxf_warnings(dxf_debug_physics_tests)
    add_test(NAME DebugPhysicsCapture COMMAND dxf_debug_physics_tests)
    set_tests_properties(DebugPhysicsCapture PROPERTIES TIMEOUT 60 LABELS "portable;physics;debug-tools")
    # NativeContract retains its comprehensive Tests/FakeDxLib header.
    # The applier appends the geometry-only supplement to that header, never shadows it.
    if(NOT TARGET dxf_transparency_tests)
        include(CMake/RenderTransparencyTests.cmake)
        dxf_add_render_transparency_tests(dxf::support)
    endif()
    if(NOT TARGET dxf_render_continuation_tests)
        include(CMake/RenderContinuationTests.cmake)
        dxf_add_render_continuation_tests(dxf::support)
    endif()
    if(NOT TARGET dxf_render_views_tests)
        include(CMake/RenderViewsTests.cmake)
        dxf_add_render_views_tests(dxf::support)
    endif()
endif()
set(_dxf_debug_default OFF)
if(DXF_BUILD_NATIVE AND DXF_BUILD_TESTS)
    set(_dxf_debug_default ON)
endif()
option(DXF_BUILD_RENDER_DEBUG "Build the RenderDebug visual verification executable" ${_dxf_debug_default})
if(DXF_BUILD_RENDER_DEBUG)
    if(NOT DXF_BUILD_NATIVE)
        message(FATAL_ERROR "DXF_BUILD_RENDER_DEBUG requires DXF_BUILD_NATIVE=ON")
    endif()
    add_executable(RenderDebug WIN32 Examples/RenderDebug/WindowsMain.cpp Examples/RenderDebug/RenderDebugScene.cpp
        Examples/RenderDebug/TransparencyDemo.cpp)
    target_link_libraries(RenderDebug PRIVATE dxf::native dxf::debug_tools)
    dxf_ide_headers(RenderDebug Examples/RenderDebug)
    dxf_warnings(RenderDebug)
    set_property(TARGET RenderDebug PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    dxf_runtime_paths(RenderDebug RenderDebug)
endif()
unset(_dxf_debug_default)
