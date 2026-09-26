# UI基盤（任意にリンクする）。dxf::uiは要素の木・レイアウト・入力の仲介・部品・スタイル・描画命令化、
# dxf::ui_runtimeはSceneへの接続（入力の前処理・描画・世界の平面パネル）。PhysicsとGameplayには依存しない。
set(DXF_UI_SOURCES
    Source/Ui/Private/Dxf/UiChoice.cpp
    Source/Ui/Private/Dxf/UiPopup.cpp
    Source/Ui/Private/Dxf/UiListView.cpp
    Source/Ui/Private/Dxf/UiScrollView.cpp
    Source/Ui/Private/Dxf/UiImage.cpp
    Source/Ui/Private/Dxf/UiProgressBar.cpp
    Source/Ui/Private/Dxf/UiSlider.cpp
    Source/Ui/Private/Dxf/UiToggle.cpp
    Source/Ui/Private/Dxf/UiRange.cpp
    Source/Ui/Private/Dxf/UiStyleResource.cpp
    Source/Ui/Private/Dxf/UiInspection.cpp
    Source/Ui/Private/Dxf/UiSurface.cpp
    Source/Ui/Private/Dxf/UiTextLayout.cpp
    Source/Ui/Private/Dxf/UiDrawContext.cpp
    Source/Ui/Private/Dxf/UiStyleSheet.cpp
    Source/Ui/Private/Dxf/UiStyleParser.cpp
    Source/Ui/Private/Dxf/UiDefaultStyles.cpp
    Source/Ui/Private/Dxf/UiTree.cpp
    Source/Ui/Private/Dxf/UiLayoutEngine.cpp
    Source/Ui/Private/Dxf/UiInputDispatcher.cpp
    Source/Ui/Private/Dxf/UiFocusManager.cpp
    Source/Ui/Private/Dxf/UiTooltipService.cpp
    Source/Ui/Private/Dxf/UiDrawBuilder.cpp
    Source/Ui/Private/Dxf/UiElement.cpp
    Source/Ui/Private/Dxf/UiRoot.cpp
    Source/Ui/Private/Dxf/UiPressable.cpp
    Source/Ui/Private/Dxf/UiButton.cpp
    Source/Ui/Private/Dxf/UiLabel.cpp
    Source/Ui/Private/Dxf/UiAssetTextService.cpp
    Source/Ui/Private/Dxf/UiRenderer.cpp
    Source/Ui/Private/Dxf/UiWorldPanel.cpp)
add_library(dxf_ui STATIC ${DXF_UI_SOURCES})
add_library(dxf::ui ALIAS dxf_ui)
set_target_properties(dxf_ui PROPERTIES EXPORT_NAME ui DEBUG_POSTFIX d)
target_link_libraries(dxf_ui PUBLIC dxf::support)
target_include_directories(dxf_ui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source/Ui/Private/Dxf")
dxf_public_headers(dxf_ui Ui PUBLIC)
dxf_warnings(dxf_ui)
list(APPEND DXF_EXPORT_TARGETS dxf_ui)

# Sceneへの接続（入力の仲介・描画・ワールドのパネル）。Runtimeの汎用の入力の仲介の窓口だけを使う。
add_library(dxf_ui_runtime STATIC
    Source/UiRuntime/Private/Dxf/UiNavigationBindings.cpp
    Source/UiRuntime/Private/Dxf/UiSceneHost.cpp
    Source/UiRuntime/Private/Dxf/UiHostDisplays.cpp
    Source/UiRuntime/Private/Dxf/UiHostInput.cpp
    Source/UiRuntime/Private/Dxf/UiHostDraw.cpp
    Source/UiRuntime/Private/Dxf/UiHostInspection.cpp)
add_library(dxf::ui_runtime ALIAS dxf_ui_runtime)
set_target_properties(dxf_ui_runtime PROPERTIES EXPORT_NAME ui_runtime DEBUG_POSTFIX d)
target_link_libraries(dxf_ui_runtime PUBLIC dxf::ui dxf::runtime)
dxf_public_headers(dxf_ui_runtime UiRuntime PUBLIC)
dxf_warnings(dxf_ui_runtime)
list(APPEND DXF_EXPORT_TARGETS dxf_ui_runtime)

if(DXF_BUILD_TESTS)
    add_executable(dxf_ui_tests Tests/TestMain.cpp
        Tests/Ui/UiLifetimeTests.cpp
        Tests/Ui/UiLayoutTests.cpp
        Tests/Ui/UiTextStyleTests.cpp
        Tests/Ui/UiContinuationTests.cpp
        Tests/Ui/UiControlTests.cpp
        Tests/Ui/UiScrollAxesTests.cpp
        Tests/Ui/UiInspectionSourceTests.cpp
        Tests/Ui/UiResourceInspectionTests.cpp)
    target_link_libraries(dxf_ui_tests PRIVATE dxf::ui)
    target_include_directories(dxf_ui_tests PRIVATE Tests)
    dxf_ide_headers(dxf_ui_tests Tests/Ui)
    dxf_warnings(dxf_ui_tests)
    add_test(NAME UiFoundation COMMAND dxf_ui_tests)
    set_tests_properties(UiFoundation PROPERTIES TIMEOUT 60 LABELS "portable;ui")
endif()

if(DXF_BUILD_TESTS)
    add_executable(dxf_ui_runtime_tests Tests/TestMain.cpp Tests/Ui/UiHostTests.cpp Tests/Ui/UiHostOwnershipTests.cpp
        Tests/Ui/UiHostCompositionTests.cpp Tests/Ui/UiHostInspectionTests.cpp)
    target_link_libraries(dxf_ui_runtime_tests PRIVATE dxf::ui_runtime)
    target_include_directories(dxf_ui_runtime_tests PRIVATE Tests)
    dxf_ide_headers(dxf_ui_runtime_tests Tests/Ui)
    dxf_warnings(dxf_ui_runtime_tests)
    add_test(NAME UiRuntime COMMAND dxf_ui_runtime_tests)
    set_tests_properties(UiRuntime PROPERTIES TIMEOUT 60 LABELS "portable;ui;runtime")
endif()

if(DXF_BUILD_TESTS)
    add_executable(dxf_ui_benchmark Tools/UiBenchmark/Main.cpp
        Source/Toolbox/Private/Toolbox/Testing/AllocationFault.cpp)
    target_include_directories(dxf_ui_benchmark PRIVATE Source/Toolbox/Private/Toolbox/Testing)
    target_compile_definitions(dxf_ui_benchmark PRIVATE DXF_ALLOCATION_FAULT_TEST_EXECUTABLE=1)
    target_link_libraries(dxf_ui_benchmark PRIVATE dxf::ui)
    dxf_ide_headers(dxf_ui_benchmark Tools/UiBenchmark Source/Toolbox/Private/Toolbox/Testing)
    dxf_warnings(dxf_ui_benchmark)
endif()

if(DXF_BUILD_TESTS)
    add_executable(dxf_ui_fault_tests Tests/TestMain.cpp Tests/Ui/UiAllocationFaultTests.cpp
        Source/Toolbox/Private/Toolbox/Testing/AllocationFault.cpp)
    target_include_directories(dxf_ui_fault_tests PRIVATE Tests Source/Toolbox/Private/Toolbox/Testing)
    target_compile_definitions(dxf_ui_fault_tests PRIVATE DXF_ALLOCATION_FAULT_TEST_EXECUTABLE=1)
    target_link_libraries(dxf_ui_fault_tests PRIVATE dxf::ui)
    dxf_ide_headers(dxf_ui_fault_tests Tests/Ui Source/Toolbox/Private/Toolbox/Testing)
    dxf_warnings(dxf_ui_fault_tests)
    add_test(NAME UiAllocationFault COMMAND dxf_ui_fault_tests)
    set_tests_properties(UiAllocationFault PROPERTIES TIMEOUT 60 LABELS "portable;ui;allocation-fault")
endif()
