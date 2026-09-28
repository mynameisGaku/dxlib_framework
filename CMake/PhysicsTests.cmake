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
        "${_root}/Tests/Physics/WorldQueryTests.cpp"
        "${_root}/Tests/Physics/WorldQuery2DTests.cpp"
        "${_root}/Tests/Physics/WorldQueryFilterTests.cpp"
        "${_root}/Tests/Physics/ColliderResponseTests.cpp"
        "${_root}/Tests/Physics/WorldEventTests.cpp"
        "${_root}/Tests/Physics/CapsuleGeometryTests.cpp"
        "${_root}/Tests/Physics/CapsuleWorldTests.cpp"
        "${_root}/Tests/Physics/CharacterCapsuleTests.cpp"
        "${_root}/Tests/Physics/CharacterPushTests.cpp"
        "${_root}/Tests/Physics/SolverBroadPhaseTests.cpp"
        "${_root}/Tests/Physics/JointLifetimeTests.cpp"
        "${_root}/Tests/Physics/DistanceJointTests.cpp"
        "${_root}/Tests/Physics/DistanceJointSleepTests.cpp"
        "${_root}/Tests/Physics/BoxWallContactTests.cpp"
        "${_root}/Tests/Physics/WorldOverlapTests.cpp"
        "${_root}/Tests/Physics/WorldSweepTests.cpp"
        "${_root}/Tests/Physics/WorldSweepNormalTests.cpp"
        "${_root}/Tests/Physics/WorldSlideTests.cpp"
        "${_root}/Tests/Physics/CharacterMovementTests.cpp"
        "${_root}/Tests/Physics/QueryTreeTests.cpp"
        "${_root}/Tests/Physics/QueryIndexEquivalenceTests.cpp"
        "${_root}/Tests/Physics/QueryIndexContractTests.cpp"
        "${_root}/Tests/Physics/QueryIndexTestSupport.h"
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
    # 遽・峇蝠上＞蜷医ｏ縺帙・邨先棡遒ｺ菫昴・謨・囿豕ｨ蜈･縲ら｢ｺ菫晉ｽｮ謠帙・縺薙・髫秘屬縺励◆螳溯｡後ヵ繧｡繧､繝ｫ縺縺代↓繝ｪ繝ｳ繧ｯ縺吶ｋ縲・
    add_executable(${Target}_overlap_fault
        "${_root}/Tests/Physics/WorldEventAllocationFaultTests.cpp"
        "${_root}/Tests/Physics/CapsuleAllocationFaultTests.cpp"
        "${_root}/Tests/Physics/OverlapAllocationFaultTests.cpp"
        "${_root}/Tests/Physics/QueryIndexAllocationFaultTests.cpp"
        "${_root}/Source/Toolbox/Private/Toolbox/Testing/AllocationFault.cpp")
    target_compile_definitions(${Target}_overlap_fault PRIVATE DXF_ALLOCATION_FAULT_TEST_EXECUTABLE=1)
    target_link_libraries(${Target}_overlap_fault PRIVATE ${CoreTarget})
    target_compile_features(${Target}_overlap_fault PRIVATE cxx_std_20)
    if(COMMAND dxf_warnings)
        dxf_warnings(${Target}_overlap_fault)
    endif()
    add_test(NAME PhysicsOverlapFault COMMAND ${Target}_overlap_fault)
    set_tests_properties(PhysicsOverlapFault PROPERTIES TIMEOUT 60 LABELS "portable;physics;fault")
endfunction()
