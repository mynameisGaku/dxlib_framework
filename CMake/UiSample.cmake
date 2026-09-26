# UIサンプル本体は正規ライブラリへ一度リンクし、試験とGUI入口で共用する。
option(DXF_BUILD_UI_SAMPLE "Build the optional 2D/3D UI development sample" OFF)
if(DXF_BUILD_TESTS OR DXF_BUILD_UI_SAMPLE OR DXF_BUILD_NATIVE_SMOKE)
    add_library(dxf_ui_sample STATIC
        Examples/UiSample/UiItemBrowser.cpp
        Examples/UiSample/UiPausePanel.cpp
        Examples/UiSample/UiPlay2DScene.cpp
        Examples/UiSample/UiPlay3DScene.cpp
        Examples/UiSample/UiPlayerHud.cpp
        Examples/UiSample/UiSampleScenes.cpp
        Examples/UiSample/UiSampleShell.cpp
        Examples/UiSample/UiSampleViews.cpp
        Examples/UiSample/UiSettingsPanel.cpp
        Examples/UiSample/UiTitleScene.cpp
        Examples/UiSample/UiTitleScreen.cpp
        Examples/UiSample/UiWorldControls.cpp
        Examples/GameplaySample/SampleCharacters.cpp
        Examples/GameplaySample/SampleLevel.cpp)
    target_link_libraries(dxf_ui_sample PUBLIC dxf::ui_runtime dxf::gameplay)
    target_include_directories(dxf_ui_sample PUBLIC Examples/UiSample)
    # 実装を持つライブラリへヘッダーとスタイルを登録し、実際のフォルダー構成で表示する。
    # GUI入口へcppを再登録しない（同じサンプル本体を二重コンパイルしない）。
    target_sources(dxf_ui_sample PRIVATE
        Examples/GameplaySample/SampleCharacters.h
        Examples/GameplaySample/SampleLevel.h)
    file(GLOB_RECURSE DXF_UI_SAMPLE_STYLES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Examples/UiSample/Styles/*.dxfui")
    if(DXF_UI_SAMPLE_STYLES)
        set_source_files_properties(${DXF_UI_SAMPLE_STYLES} PROPERTIES HEADER_FILE_ONLY TRUE)
        target_sources(dxf_ui_sample PRIVATE ${DXF_UI_SAMPLE_STYLES})
    endif()
    dxf_ide_headers(dxf_ui_sample Examples/UiSample)
    dxf_warnings(dxf_ui_sample)
endif()
if(DXF_BUILD_UI_SAMPLE)
    if(NOT DXF_BUILD_NATIVE)
        message(FATAL_ERROR "DXF_BUILD_UI_SAMPLE requires DXF_BUILD_NATIVE=ON")
    endif()
    add_executable(UISample WIN32 Examples/UiSample/WindowsMain.cpp)
    target_link_libraries(UISample PRIVATE dxf_ui_sample dxf::native)
    dxf_warnings(UISample)
    dxf_ide_headers(UISample Examples/UiSample)
    set_property(TARGET UISample PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    dxf_runtime_paths(UISample UISample)
endif()

if(DXF_BUILD_TESTS)
    add_executable(dxf_ui_application_tests Tests/TestMain.cpp Tests/Ui/UiApplicationTests.cpp Tests/UiNativeSmoke/ForwardRenderer.cpp)
    target_include_directories(dxf_ui_application_tests PRIVATE Tests Tests/Ui)
    target_link_libraries(dxf_ui_application_tests PRIVATE dxf_ui_sample)
    dxf_ide_headers(dxf_ui_application_tests Tests/Ui Tests/UiNativeSmoke)
    dxf_warnings(dxf_ui_application_tests)
    add_test(NAME UiApplication COMMAND dxf_ui_application_tests)
    set_tests_properties(UiApplication PROPERTIES TIMEOUT 120 LABELS "portable;ui;application")
endif()

if(DXF_BUILD_NATIVE_SMOKE)
    add_executable(NativeUiSmoke Tests/UiNativeSmoke/Main.cpp
        Tests/UiNativeSmoke/ForwardRenderer.cpp Tests/UiNativeSmoke/NativePixels.cpp
        Tests/UiNativeSmoke/DisplayScene.cpp Tests/UiNativeSmoke/DisplayScenario.cpp Tests/UiNativeSmoke/ScreenCapture.cpp)
    target_link_libraries(NativeUiSmoke PRIVATE dxf_ui_sample dxf::native)
    target_compile_definitions(NativeUiSmoke PRIVATE DX_NON_USING_NAMESPACE_DXLIB NOMINMAX)
    if(MSVC)
        target_compile_options(NativeUiSmoke PRIVATE /UUNICODE /U_UNICODE)
    endif()
    dxf_ide_headers(NativeUiSmoke Tests/UiNativeSmoke)
    dxf_warnings(NativeUiSmoke)
    set_target_properties(NativeUiSmoke PROPERTIES
        VS_DEBUGGER_COMMAND_ARGUMENTS "\"${CMAKE_CURRENT_SOURCE_DIR}\" \"${CMAKE_CURRENT_BINARY_DIR}/ui-smoke-vs-$<CONFIG>\""
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:NativeUiSmoke>")
    if(DXF_RUN_DEVICE_TESTS)
        add_test(NAME NativeUiDeviceSmoke COMMAND NativeUiSmoke "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_BINARY_DIR}/ui-smoke-$<CONFIG>")
        set_tests_properties(NativeUiDeviceSmoke PROPERTIES TIMEOUT 180 LABELS "real-sdk;device;ui;application")
    endif()
endif()
