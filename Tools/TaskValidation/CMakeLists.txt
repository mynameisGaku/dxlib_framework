cmake_minimum_required(VERSION 3.24)
project(DxfTaskDispatcherRecovery LANGUAGES CXX)
get_filename_component(DXF_TASK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
option(DXF_TASK_SANITIZERS "Address and undefined sanitizers (Clang/GCC)" OFF)
option(DXF_TASK_THREAD_SANITIZER "Thread sanitizer (Clang/GCC)" OFF)
if(DXF_TASK_SANITIZERS AND DXF_TASK_THREAD_SANITIZER)
    message(FATAL_ERROR "AddressSanitizer and ThreadSanitizer need separate builds")
endif()
find_package(Threads REQUIRED)
add_executable(dxf_task_dispatcher_recovery_tests
    Main.cpp
    "${DXF_TASK_ROOT}/Tests/TaskDispatcherRecoveryTests.cpp"
    "${DXF_TASK_ROOT}/Source/Runtime/Private/Dxf/TaskDispatcher.cpp"
    "${DXF_TASK_ROOT}/Source/Toolbox/Private/Toolbox/JobSystem.cpp"
    "${DXF_TASK_ROOT}/Source/Toolbox/Private/Toolbox/Threading.cpp"
    "${DXF_TASK_ROOT}/Source/Toolbox/Private/Toolbox/Testing/AllocationFault.cpp")
target_compile_features(dxf_task_dispatcher_recovery_tests PRIVATE cxx_std_20)
set_target_properties(dxf_task_dispatcher_recovery_tests PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(dxf_task_dispatcher_recovery_tests PRIVATE
    "${DXF_TASK_ROOT}/Source/Runtime/Public"
    "${DXF_TASK_ROOT}/Source/Toolbox/Public"
    "${DXF_TASK_ROOT}/Source/Toolbox/Private"
    "${DXF_TASK_ROOT}/Tests")
target_compile_definitions(dxf_task_dispatcher_recovery_tests PRIVATE DXF_ALLOCATION_FAULT_TEST_EXECUTABLE=1)
target_link_libraries(dxf_task_dispatcher_recovery_tests PRIVATE Threads::Threads)
if(MSVC)
    target_compile_options(dxf_task_dispatcher_recovery_tests PRIVATE /W4 /WX /utf-8 /permissive- /EHsc)
else()
    target_compile_options(dxf_task_dispatcher_recovery_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
    if(DXF_TASK_SANITIZERS)
        target_compile_options(dxf_task_dispatcher_recovery_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(dxf_task_dispatcher_recovery_tests PRIVATE -fsanitize=address,undefined)
    endif()
    if(DXF_TASK_THREAD_SANITIZER)
        target_compile_options(dxf_task_dispatcher_recovery_tests PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(dxf_task_dispatcher_recovery_tests PRIVATE -fsanitize=thread)
    endif()
endif()
enable_testing()
file(STRINGS "${DXF_TASK_ROOT}/Tests/TaskDispatcherRecoveryTests.cpp" CaseLines REGEX "^TEST\\(\"td_[^\"]+\"\\)")
foreach(CaseLine IN LISTS CaseLines)
    string(REGEX REPLACE "^TEST\\(\"([^\"]+)\"\\).*" "\\1" CaseName "${CaseLine}")
    add_test(NAME "${CaseName}" COMMAND dxf_task_dispatcher_recovery_tests "${CaseName}")
    set_tests_properties("${CaseName}" PROPERTIES TIMEOUT 10 LABELS "task-dispatcher;focused;not-application")
endforeach()
