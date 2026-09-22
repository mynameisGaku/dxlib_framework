# 実装ライブラリだけを差し替え、同じ実Application回帰を登録する。
function(dxf_add_scene_task_integration_tests Target RuntimeLibrary)
    get_filename_component(_test_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    add_executable(${Target}
        "${_test_root}/Tools/SceneTaskValidation/Main.cpp"
        "${_test_root}/Tests/SceneTaskIntegrationTests.cpp")
    target_link_libraries(${Target} PRIVATE ${RuntimeLibrary})
    target_include_directories(${Target} PRIVATE "${_test_root}/Tests")
    target_compile_features(${Target} PRIVATE cxx_std_20)
    set_target_properties(${Target} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        # C4324はRenderer公開ヘッダーのalignas(16)による意図したパディング通知。その警告だけを除外する。
        target_compile_options(${Target} PRIVATE /W4 /WX /wd4324 /utf-8 /permissive- /EHsc)
    else()
        target_compile_options(${Target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
    endif()
    file(STRINGS "${_test_root}/Tests/SceneTaskIntegrationTests.cpp" _case_lines REGEX "^TEST\\(\"scene_task_[^\"]+\"\\)")
    foreach(_line IN LISTS _case_lines)
        string(REGEX REPLACE "^TEST\\(\"([^\"]+)\"\\).*" "\\1" _name "${_line}")
        add_test(NAME "${_name}" COMMAND ${Target} "${_name}")
        set_tests_properties("${_name}" PROPERTIES TIMEOUT 20 LABELS "scene-task;application-integration")
    endforeach()
endfunction()
