cmake_minimum_required(VERSION 3.24)
project(dxlib_framework VERSION 0.1.0 LANGUAGES CXX)
option(DXF_BUILD_TESTS "Build portable regression tests" ${PROJECT_IS_TOP_LEVEL})
option(DXF_BUILD_NATIVE "Build the Windows DxLib adapter" OFF)
option(DXF_BUILD_EXAMPLE "Build the native Sandbox" OFF)
option(DXF_SANITIZERS "Enable address/undefined sanitizers" OFF)
set(DXLIB_ROOT "" CACHE PATH "Directory containing DxLib.h and the matching libraries")
add_library(dxlib_framework STATIC Source/Foundation/Private/Foundation.cpp
    Source/DxLibSupport/Private/Assets.cpp
    Source/DxLibSupport/Private/Render.cpp
    Source/DxLibSupport/Private/Audio.cpp
    Source/Runtime/Private/LifecycleObject.cpp
    Source/Runtime/Private/SceneNavigator.cpp
    Source/Runtime/Private/Application.cpp)
add_library(dxf::framework ALIAS dxlib_framework)
target_compile_features(dxlib_framework PUBLIC cxx_std_20)
foreach(Layer Foundation DxLibSupport Runtime Gameplay)
    target_include_directories(dxlib_framework PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source/${Layer}/Public>")
endforeach()
function(dxf_warnings Target)
    if(MSVC)
        target_compile_options(${Target} PRIVATE /W4 /permissive- /utf-8 /EHsc /GR)
    else()
        target_compile_options(${Target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endfunction()
dxf_warnings(dxlib_framework)
if(DXF_SANITIZERS AND NOT MSVC)
    target_compile_options(dxlib_framework PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(dxlib_framework PUBLIC -fsanitize=address,undefined)
endif()
set(DXF_NATIVE_SOURCES
    Source/Native/Private/DxLibPlatform.cpp
    Source/Native/Private/DxLibInputSource.cpp
    Source/Native/Private/DxLibTextureBackend.cpp
    Source/Native/Private/DxLibSoundBackend.cpp
    Source/Native/Private/DxLibFontBackend.cpp
    Source/Native/Private/DxLibRenderBackend.cpp
)
if(DXF_BUILD_TESTS)
    enable_testing()
    add_executable(dxf_tests Tests/TestMain.cpp Tests/FoundationTests.cpp Tests/SupportTests.cpp Tests/GameplayTests.cpp Tests/ApplicationTests.cpp)
    target_link_libraries(dxf_tests PRIVATE dxf::framework)
    target_include_directories(dxf_tests PRIVATE Tests)
    dxf_warnings(dxf_tests)
    add_test(NAME Framework COMMAND dxf_tests)
    add_executable(dxf_native_contract_tests Tests/TestMain.cpp Tests/NativeTests.cpp ${DXF_NATIVE_SOURCES})
    target_link_libraries(dxf_native_contract_tests PRIVATE dxf::framework)
    target_include_directories(dxf_native_contract_tests PRIVATE Tests Tests/FakeDxLib Source/Native/Public)
    dxf_warnings(dxf_native_contract_tests)
    add_test(NAME NativeContract COMMAND dxf_native_contract_tests)
endif()
