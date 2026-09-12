cmake_minimum_required(VERSION 3.24)
project(dxlib_framework VERSION 0.2.0 LANGUAGES CXX)

if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

option(DXF_BUILD_TESTS "Build portable and native-contract regression tests" ${PROJECT_IS_TOP_LEVEL})
option(DXF_BUILD_NATIVE "Build the Windows/MSVC DxLib adapter" OFF)
option(DXF_BUILD_NATIVE_SMOKE "Build the self-terminating real-SDK smoke executable" ${DXF_BUILD_NATIVE})
option(DXF_RUN_DEVICE_TESTS "Register smoke tests requiring a Windows desktop and devices" OFF)
option(DXF_BUILD_EXAMPLE "Build the Windows Sandbox" OFF)
option(DXF_INSTALL "Generate installation and package-export rules" ${PROJECT_IS_TOP_LEVEL})
option(DXF_SANITIZERS "Enable address/undefined sanitizers" OFF)
option(DXF_COVERAGE "Enable GCC source coverage instrumentation" OFF)
option(DXF_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)
set(DXLIB_ROOT "" CACHE PATH "Official VC SDK root or directory containing DxLib.h and libraries")
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC AND DXF_BUILD_NATIVE)
    # The official VC SDK uses the static CRT. This also covers our examples and tests.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

function(dxf_warnings Target)
    if(MSVC)
        target_compile_options(${Target} PRIVATE /W4 /permissive- /utf-8 /EHsc /GR)
        if(DXF_WARNINGS_AS_ERRORS)
            target_compile_options(${Target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${Target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        if(DXF_WARNINGS_AS_ERRORS)
            target_compile_options(${Target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(dxf_public_headers Target Layer Visibility)
    target_include_directories(${Target} ${Visibility}
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source/${Layer}/Public>"
        "$<INSTALL_INTERFACE:include>")
endfunction()

add_library(dxf_foundation INTERFACE)
add_library(dxf::foundation ALIAS dxf_foundation)
set_target_properties(dxf_foundation PROPERTIES EXPORT_NAME foundation)
target_compile_features(dxf_foundation INTERFACE cxx_std_20)
dxf_public_headers(dxf_foundation Foundation INTERFACE)
if(DXF_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "The address+undefined sanitizer profile requires GCC or Clang on Linux")
    endif()
    target_compile_options(dxf_foundation INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(dxf_foundation INTERFACE -fsanitize=address,undefined)
endif()
if(DXF_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "DXF_COVERAGE requires GCC/gcov")
    endif()
    target_compile_options(dxf_foundation INTERFACE --coverage)
    target_link_options(dxf_foundation INTERFACE --coverage)
endif()

add_library(dxf_support STATIC
    Source/DxLibSupport/Private/AssetService.cpp
    Source/DxLibSupport/Private/TextureLoader.cpp
    Source/DxLibSupport/Private/SoundLoader.cpp
    Source/DxLibSupport/Private/FontLoader.cpp
    Source/DxLibSupport/Private/RenderSystem2D.cpp
    Source/DxLibSupport/Private/RenderQueue2D.cpp
    Source/DxLibSupport/Private/AudioPlayer.cpp)
add_library(dxf::support ALIAS dxf_support)
set_target_properties(dxf_support PROPERTIES EXPORT_NAME support DEBUG_POSTFIX d)
target_link_libraries(dxf_support PUBLIC dxf::foundation)
dxf_public_headers(dxf_support DxLibSupport PUBLIC)
dxf_warnings(dxf_support)

add_library(dxf_runtime STATIC
    Source/Runtime/Private/LifecycleObject.cpp
    Source/Runtime/Private/SceneNavigator.cpp
    Source/Runtime/Private/Application.cpp)
add_library(dxf::runtime ALIAS dxf_runtime)
set_target_properties(dxf_runtime PROPERTIES EXPORT_NAME runtime DEBUG_POSTFIX d)
target_link_libraries(dxf_runtime PUBLIC dxf::support)
dxf_public_headers(dxf_runtime Runtime PUBLIC)
dxf_warnings(dxf_runtime)

add_library(dxf_gameplay INTERFACE)
add_library(dxf::gameplay ALIAS dxf_gameplay)
set_target_properties(dxf_gameplay PROPERTIES EXPORT_NAME gameplay)
target_link_libraries(dxf_gameplay INTERFACE dxf::runtime)
dxf_public_headers(dxf_gameplay Gameplay INTERFACE)

add_library(dxlib_framework INTERFACE)
add_library(dxf::framework ALIAS dxlib_framework)
set_target_properties(dxlib_framework PROPERTIES EXPORT_NAME framework)
target_link_libraries(dxlib_framework INTERFACE dxf::gameplay)

set(DXF_NATIVE_SOURCES
    Source/Native/Private/DxLibPlatform.cpp
    Source/Native/Private/DxLibInputSource.cpp
    Source/Native/Private/DxLibTextureBackend.cpp
    Source/Native/Private/DxLibSoundBackend.cpp
    Source/Native/Private/DxLibFontBackend.cpp
    Source/Native/Private/DxLibRenderBackend.cpp)

if(DXF_BUILD_TESTS)
    enable_testing()
    add_executable(dxf_tests Tests/TestMain.cpp Tests/FoundationTests.cpp Tests/SupportTests.cpp
        Tests/GameplayTests.cpp Tests/ApplicationTests.cpp Tests/SandboxTests.cpp
        Tests/HardeningTests.cpp Tests/UsabilityTests.cpp Tests/StressTests.cpp Examples/Sandbox/SandboxGame.cpp)
    target_link_libraries(dxf_tests PRIVATE dxf::framework)
    target_include_directories(dxf_tests PRIVATE Tests Examples/Sandbox)
    dxf_warnings(dxf_tests)
    add_test(NAME Framework COMMAND dxf_tests)
    set_tests_properties(Framework PROPERTIES TIMEOUT 60 LABELS "portable")

    add_executable(dxf_native_contract_tests Tests/TestMain.cpp Tests/NativeTests.cpp Tests/NativeMacroTests.cpp Tests/NativeSmoke/Smoke.cpp ${DXF_NATIVE_SOURCES})
    target_link_libraries(dxf_native_contract_tests PRIVATE dxf::framework)
    target_include_directories(dxf_native_contract_tests PRIVATE Tests Tests/FakeDxLib Source/Native/Public)
    target_compile_definitions(dxf_native_contract_tests PRIVATE DXF_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Assets")
    dxf_warnings(dxf_native_contract_tests)
    add_test(NAME NativeContract COMMAND dxf_native_contract_tests)
    set_tests_properties(NativeContract PROPERTIES TIMEOUT 60 LABELS "contract;not-real-sdk")
endif()

set(DXF_EXPORT_TARGETS dxlib_framework dxf_foundation dxf_support dxf_runtime dxf_gameplay)
if(DXF_BUILD_NATIVE)
    if(NOT WIN32 OR NOT MSVC)
        message(FATAL_ERROR "DXF_BUILD_NATIVE requires Windows/MSVC x64. Portable tests do not need the SDK.")
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Use an x64 Visual Studio developer environment")
    endif()
    list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")
    find_package(DxLib REQUIRED)
    add_library(dxf_native_backends STATIC ${DXF_NATIVE_SOURCES})
    add_library(dxf::native_backends ALIAS dxf_native_backends)
    set_target_properties(dxf_native_backends PROPERTIES EXPORT_NAME native_backends DEBUG_POSTFIX d)
    target_link_libraries(dxf_native_backends PUBLIC dxf::support DxLib::SDK)
    dxf_public_headers(dxf_native_backends Native PUBLIC)
    target_compile_definitions(dxf_native_backends PRIVATE DX_NON_USING_NAMESPACE_DXLIB NOMINMAX)
    target_compile_options(dxf_native_backends PRIVATE /UUNICODE /U_UNICODE)
    dxf_warnings(dxf_native_backends)

    add_library(dxf_native INTERFACE)
    add_library(dxf::native ALIAS dxf_native)
    set_target_properties(dxf_native PROPERTIES EXPORT_NAME native)
    target_link_libraries(dxf_native INTERFACE dxf::native_backends dxf::framework)
    list(APPEND DXF_EXPORT_TARGETS dxf_native_backends dxf_native)
endif()

if(DXF_BUILD_NATIVE_SMOKE)
    if(NOT DXF_BUILD_NATIVE)
        message(FATAL_ERROR "DXF_BUILD_NATIVE_SMOKE requires DXF_BUILD_NATIVE=ON")
    endif()
    add_executable(NativeSmoke Tests/NativeSmoke/Main.cpp Tests/NativeSmoke/Smoke.cpp)
    target_link_libraries(NativeSmoke PRIVATE dxf::native_backends)
    dxf_warnings(NativeSmoke)
    if(DXF_RUN_DEVICE_TESTS)
        enable_testing()
        add_test(NAME NativeDeviceSmoke COMMAND NativeSmoke)
        set_tests_properties(NativeDeviceSmoke PROPERTIES TIMEOUT 60 LABELS "real-sdk;device"
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:NativeSmoke>")
    endif()
elseif(DXF_RUN_DEVICE_TESTS)
    message(FATAL_ERROR "DXF_RUN_DEVICE_TESTS requires DXF_BUILD_NATIVE_SMOKE=ON")
endif()

if(DXF_BUILD_EXAMPLE)
    if(NOT DXF_BUILD_NATIVE)
        message(FATAL_ERROR "DXF_BUILD_EXAMPLE requires DXF_BUILD_NATIVE=ON")
    endif()
    add_executable(Sandbox WIN32 Examples/Sandbox/WindowsMain.cpp Examples/Sandbox/SandboxGame.cpp)
    target_link_libraries(Sandbox PRIVATE dxf::native)
    dxf_warnings(Sandbox)
    set_property(TARGET Sandbox PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Sandbox>")
endif()

# One writer for shared assets; parallel links must not copy into the same destination.
if(TARGET NativeSmoke OR TARGET Sandbox)
    if(TARGET Sandbox)
        set(_dxf_asset_target Sandbox)
    else()
        set(_dxf_asset_target NativeSmoke)
    endif()
    add_custom_target(dxf_sample_assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Assets"
            "$<TARGET_FILE_DIR:${_dxf_asset_target}>/Assets"
        COMMENT "Copying sample assets")
    foreach(_dxf_executable NativeSmoke Sandbox)
        if(TARGET ${_dxf_executable})
            add_dependencies(${_dxf_executable} dxf_sample_assets)
        endif()
    endforeach()
endif()

if(DXF_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    install(TARGETS ${DXF_EXPORT_TARGETS} EXPORT dxlib_frameworkTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    foreach(Layer Foundation DxLibSupport Runtime Gameplay Native)
        install(DIRECTORY "Source/${Layer}/Public/" DESTINATION include FILES_MATCHING PATTERN "*.h")
    endforeach()
    set(DXF_CONFIG_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/dxlib_framework")
    configure_package_config_file(CMake/dxlib_frameworkConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/dxlib_frameworkConfig.cmake"
        INSTALL_DESTINATION "${DXF_CONFIG_DESTINATION}")
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/dxlib_frameworkConfigVersion.cmake"
        VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
    install(EXPORT dxlib_frameworkTargets NAMESPACE dxf:: DESTINATION "${DXF_CONFIG_DESTINATION}")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dxlib_frameworkConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/dxlib_frameworkConfigVersion.cmake" CMake/FindDxLib.cmake
        DESTINATION "${DXF_CONFIG_DESTINATION}")
    install(FILES README.md DESTINATION "${CMAKE_INSTALL_DATADIR}/dxlib_framework")
endif()
