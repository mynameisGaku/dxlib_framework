# Shared registration for the full repository and the standalone validation project.
function(dxf_add_physics_tests Target CoreTarget)
    get_filename_component(_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    add_executable(${Target}
        "${_root}/Tools/PhysicsValidation/Main.cpp"
        "${_root}/Tests/Physics/CollisionTests.cpp"
        "${_root}/Tests/Physics/SweepTests.cpp"
        "${_root}/Tests/Physics/FixedStepTests.cpp"
        "${_root}/Tests/Physics/RigidBodyTests.cpp"
        "${_root}/Tests/Physics/ContactTests.cpp"
        "${_root}/Tests/Physics/SolverTests.cpp"
        "${_root}/Tests/Physics/ContinuousTests.cpp"
        "${_root}/Tests/Physics/StabilityTests.cpp"
        "${_root}/Tests/Physics/ParallelTests.cpp"
        "${_root}/Tests/Physics/TestCases.h")
    target_include_directories(${Target} PRIVATE "${_root}/Tests/Physics" "${_root}/Source/Physics/Private")
    target_include_directories(${Target} PRIVATE "${_root}/Tests/Physics" "${_root}/Source/Physics/Private")
    target_link_libraries(${Target} PRIVATE ${CoreTarget})
    target_compile_features(${Target} PRIVATE cxx_std_20)
    if(DXF_SANITIZERS)
        target_compile_options(${Target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${Target} PRIVATE -fsanitize=address,undefined)
    endif()
    if(COMMAND dxf_ide_headers)
        dxf_ide_headers(${Target} Tests/Physics)
    endif()
    add_test(NAME PhysicsContinuation COMMAND ${Target})
    set_tests_properties(PhysicsContinuation PROPERTIES TIMEOUT 180 LABELS "portable;physics")
endfunction()
