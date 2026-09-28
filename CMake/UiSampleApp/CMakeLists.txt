# SPDX-License-Identifier: NOASSERTION
# UIサンプルのGUI入口（UISample）。サンプル本体はdxf_ui_sampleが一度だけコンパイルする。
# このディレクトリのスコープだけで本体のcppをHEADER_FILE_ONLYにして載せ、IDEのUISampleからも本体へ辿れるようにする
# （ソースファイルの属性はディレクトリ単位なので、ルートで定義したdxf_ui_sampleのコンパイルには影響しない）。
set(_dxf_root "${PROJECT_SOURCE_DIR}")
add_executable(UISample WIN32 "${_dxf_root}/Examples/UiSample/WindowsMain.cpp")
target_link_libraries(UISample PRIVATE dxf_ui_sample dxf::native)
dxf_warnings(UISample)
dxf_enable_dpi_awareness(UISample)
set_property(TARGET UISample PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${_dxf_root}")
dxf_runtime_paths(UISample UISample)
# 表示専用の本体（cpp・ヘッダー・スタイル）。
get_target_property(_dxf_sample_sources dxf_ui_sample SOURCES)
set(_dxf_view_sources)
foreach(_dxf_source IN LISTS _dxf_sample_sources)
    if(IS_ABSOLUTE "${_dxf_source}")
        list(APPEND _dxf_view_sources "${_dxf_source}")
    else()
        list(APPEND _dxf_view_sources "${_dxf_root}/${_dxf_source}")
    endif()
endforeach()
file(GLOB_RECURSE _dxf_sample_headers CONFIGURE_DEPENDS "${_dxf_root}/Examples/UiSample/*.h")
list(APPEND _dxf_view_sources ${_dxf_sample_headers})
list(REMOVE_DUPLICATES _dxf_view_sources)
set_source_files_properties(${_dxf_view_sources} PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(UISample PRIVATE ${_dxf_view_sources})
get_target_property(_dxf_app_sources UISample SOURCES)
source_group(TREE "${_dxf_root}" FILES ${_dxf_app_sources})
