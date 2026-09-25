# UI基盤（任意にリンクする）。dxf::uiは要素の木・レイアウト・入力の仲介・部品・スタイル・描画命令化、
# dxf::ui_runtimeはSceneへの接続（入力の前処理・描画・世界の平面パネル）。PhysicsとGameplayには依存しない。
set(DXF_UI_SOURCES
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
    Source/Ui/Private/Dxf/UiLabel.cpp
    Source/Ui/Private/Dxf/UiAssetTextService.cpp)
add_library(dxf_ui STATIC ${DXF_UI_SOURCES})
add_library(dxf::ui ALIAS dxf_ui)
set_target_properties(dxf_ui PROPERTIES EXPORT_NAME ui DEBUG_POSTFIX d)
target_link_libraries(dxf_ui PUBLIC dxf::support)
target_include_directories(dxf_ui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source/Ui/Private/Dxf")
dxf_public_headers(dxf_ui Ui PUBLIC)
dxf_warnings(dxf_ui)
list(APPEND DXF_EXPORT_TARGETS dxf_ui)

if(DXF_BUILD_TESTS)
    add_executable(dxf_ui_tests Tests/TestMain.cpp
        Tests/Ui/UiLifetimeTests.cpp
        Tests/Ui/UiLayoutTests.cpp
        Tests/Ui/UiTextStyleTests.cpp)
    target_link_libraries(dxf_ui_tests PRIVATE dxf::ui)
    target_include_directories(dxf_ui_tests PRIVATE Tests)
    dxf_ide_headers(dxf_ui_tests Tests/Ui)
    dxf_warnings(dxf_ui_tests)
    add_test(NAME UiFoundation COMMAND dxf_ui_tests)
    set_tests_properties(UiFoundation PROPERTIES TIMEOUT 60 LABELS "portable;ui")
endif()
