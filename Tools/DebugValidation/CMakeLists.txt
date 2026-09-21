cmake_minimum_required(VERSION 3.24)
project(dxf_debug_validation LANGUAGES CXX)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CMAKE_CXX_EXTENSIONS OFF)
option(DXF_DEBUG_ASAN "Address/undefined instrumentation" OFF)
option(DXF_DEBUG_TSAN "Thread instrumentation (separate build)" OFF)
if(DXF_DEBUG_ASAN AND DXF_DEBUG_TSAN)
    message(FATAL_ERROR "Use separate sanitizer builds")
endif()
find_package(Threads REQUIRED)
add_library(core STATIC
 ${ROOT}/Source/Toolbox/Private/Toolbox/Threading.cpp
 ${ROOT}/Source/Toolbox/Private/Toolbox/JobSystem.cpp
 ${ROOT}/Source/Toolbox/Private/Toolbox/Vector3.cpp
 ${ROOT}/Source/Toolbox/Private/Toolbox/FixedStepScheduler.cpp
 ${ROOT}/Source/DxLibSupport/Private/Dxf/RenderPass3D.cpp
 ${ROOT}/Source/DxLibSupport/Private/Dxf/RenderGeometry3D.cpp
 ${ROOT}/Source/DxLibSupport/Private/Dxf/DebugDrawAdapters.cpp
 ${ROOT}/Source/DxLibSupport/Private/Dxf/Render2DContext.cpp
 ${ROOT}/Source/DxLibSupport/Private/Dxf/Render3DContext.cpp
 ${ROOT}/Source/DxLibSupport/Private/Dxf/RenderQueue2D.cpp
 ${ROOT}/Source/DxLibSupport/Private/Dxf/RenderSystem.cpp
 ${ROOT}/Source/Debug/Private/Dxf/DebugCamera3D.cpp
 ${ROOT}/Source/Debug/Private/Dxf/DebugStepController.cpp
 ${ROOT}/Source/Debug/Private/Dxf/DebugSnapshotHistory.cpp
 ${ROOT}/Source/Debug/Private/Dxf/PhysicsDebugSnapshot3D.cpp
 ${ROOT}/Source/Debug/Private/Dxf/PhysicsDebugDisplay3D.cpp)
target_include_directories(core PUBLIC ${ROOT}/Source/Toolbox/Public ${ROOT}/Source/Foundation/Public
 ${ROOT}/Source/DxLibSupport/Public ${ROOT}/Source/Debug/Public)
target_link_libraries(core PUBLIC Threads::Threads)
target_compile_features(core PUBLIC cxx_std_20)
if(MSVC)
 target_compile_options(core PUBLIC /W4 /permissive- /utf-8 /EHsc /GR)
else()
 target_compile_options(core PUBLIC -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
 if(DXF_DEBUG_ASAN)
  target_compile_options(core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(core PUBLIC -fsanitize=address,undefined)
 elseif(DXF_DEBUG_TSAN)
  target_compile_options(core PUBLIC -fsanitize=thread -fno-omit-frame-pointer)
  target_link_options(core PUBLIC -fsanitize=thread)
 endif()
endif()
enable_testing()
add_executable(debug_tests ${ROOT}/Tools/RenderValidation/Main.cpp ${ROOT}/Tests/DebugToolsTests.cpp ${ROOT}/Tests/DebugRendererIntegrationTests.cpp)
target_include_directories(debug_tests PRIVATE ${ROOT}/Tests)
target_link_libraries(debug_tests PRIVATE core)
add_test(NAME DebugTools COMMAND debug_tests)
set_tests_properties(DebugTools PROPERTIES TIMEOUT 60)
include(${ROOT}/CMake/RenderContinuationTests.cmake)
dxf_add_render_continuation_tests(core)
include(${ROOT}/CMake/RenderViewsTests.cmake)
dxf_add_render_views_tests(core)
include(${ROOT}/CMake/RenderTransparencyTests.cmake)
dxf_add_render_transparency_tests(core)
