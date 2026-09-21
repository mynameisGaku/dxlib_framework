cmake_minimum_required(VERSION 3.24)
project(dxf_render_validation LANGUAGES CXX)
set(CMAKE_CXX_EXTENSIONS OFF)
get_filename_component(DXF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
option(DXF_RENDER_ASAN "Address/undefined instrumentation" OFF)
option(DXF_RENDER_TSAN "Thread instrumentation (separate from ASan)" OFF)
if(DXF_RENDER_ASAN AND DXF_RENDER_TSAN)
    message(FATAL_ERROR "Use separate builds for ASan and TSan")
endif()
if(MSVC AND (DXF_RENDER_ASAN OR DXF_RENDER_TSAN))
    message(FATAL_ERROR "These combined sanitizer profiles require GCC/Clang on a supported platform")
endif()
find_package(Threads REQUIRED)
add_library(dxf_render_validation_core STATIC
    "${DXF_ROOT}/Source/Toolbox/Private/Toolbox/Threading.cpp"
    "${DXF_ROOT}/Source/Toolbox/Private/Toolbox/JobSystem.cpp"
    "${DXF_ROOT}/Source/DxLibSupport/Private/Dxf/RenderGeometry3D.cpp"
    "${DXF_ROOT}/Source/DxLibSupport/Private/Dxf/Render2DContext.cpp"
    "${DXF_ROOT}/Source/DxLibSupport/Private/Dxf/Render3DContext.cpp"
    "${DXF_ROOT}/Source/DxLibSupport/Private/Dxf/DebugDrawAdapters.cpp"
    "${DXF_ROOT}/Source/DxLibSupport/Private/Dxf/RenderQueue2D.cpp"
    "${DXF_ROOT}/Source/DxLibSupport/Private/Dxf/RenderSystem.cpp")
target_include_directories(dxf_render_validation_core PUBLIC
    "${DXF_ROOT}/Source/Toolbox/Public"
    "${DXF_ROOT}/Source/Foundation/Public"
    "${DXF_ROOT}/Source/DxLibSupport/Public")
target_link_libraries(dxf_render_validation_core PUBLIC Threads::Threads)
target_compile_features(dxf_render_validation_core PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(dxf_render_validation_core PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /GR)
else()
    target_compile_options(dxf_render_validation_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
    # PUBLIC makes test translation units and final executables use the same instrumentation.
    if(DXF_RENDER_ASAN)
        target_compile_options(dxf_render_validation_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(dxf_render_validation_core PUBLIC -fsanitize=address,undefined)
    elseif(DXF_RENDER_TSAN)
        target_compile_options(dxf_render_validation_core PUBLIC -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(dxf_render_validation_core PUBLIC -fsanitize=thread)
    endif()
endif()
enable_testing()
include("${DXF_ROOT}/CMake/RenderContinuationTests.cmake")
dxf_add_render_continuation_tests(dxf_render_validation_core)
# Reuse the unmodified upstream regression file; missing prerequisites must fail configure.
add_executable(dxf_threading_continuation_tests Main.cpp "${DXF_ROOT}/Tests/ThreadingTests.cpp")
target_include_directories(dxf_threading_continuation_tests PRIVATE "${DXF_ROOT}/Tests")
target_link_libraries(dxf_threading_continuation_tests PRIVATE dxf_render_validation_core)
if(MSVC)
    target_compile_options(dxf_threading_continuation_tests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /GR)
else()
    target_compile_options(dxf_threading_continuation_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
endif()
add_test(NAME ThreadingContinuation COMMAND dxf_threading_continuation_tests)
set_tests_properties(ThreadingContinuation PROPERTIES TIMEOUT 60 LABELS "portable;threading")
