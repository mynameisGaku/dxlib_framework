# dxlib_framework Multithreading Stage 2 — self audit

Base branch: `physics/continuation-89ec1f0`  
Required base commit: `c75e7bb85a33865ba6b4d07c0064646d2c402408`

## Implemented

Stage 1 is included as a prerequisite patch and provides the STL-free Toolbox threading / job foundation.

Stage 2 adds:

- `Dxf::FPhysicsExecutionSettings` / `FPhysicsExecutionDiagnostics`
- external/non-owning `Toolbox::FJobSystem` use by Physics World
- deterministic Sweep-and-Prune broad phase
- ContactSlop-aware conservative candidate bounds
- parallel broad-phase chunk scanning
- parallel narrow phase with one result slot per candidate
- deterministic candidate merge by collider index
- deterministic Dynamic contact-island construction
- same-tick wake propagation across a Dynamic island
- angular wake threshold handling
- parallel solver execution across independent Dynamic islands
- parallel velocity and discrete position/orientation integration
- atomic world-ID generation
- no writes to Static/Kinematic velocity/angular velocity from the parallel impulse path
- focused parallel regression tests and TDD logs

The public `FPhysicsWorld2D/3D` API itself remains externally serialized. Internal jobs are joined before `Step()` returns.

## Self-review findings fixed before packaging

1. **ContactSlop broad-phase omission** — initial integration used exact AABB overlap. Narrow phase accepts separation within `ContactSlop`, so a near-but-not-overlapping AABB could have been discarded. Broad phase now expands both bounds by the supplied margin.
2. **f32 broad-phase bounds** — initial core stored bounds as `f32`, which could lose range/precision at large coordinates. Internal broad-phase bounds now use `f64`.
3. **shared Static/Kinematic writes** — independent Dynamic islands can touch the same floor. The solver path must not even write `+= 0` into shared non-Dynamic bodies. Integration patch only writes impulse results when effective inverse mass/inertia is non-zero.
4. **same-tick island wake** — an explicit impulse/velocity change on one sleeping stack member must wake connected Dynamic bodies before solving. Stage 2 seeds wake from already-awake bodies and contact motion, then wakes the full Dynamic island.
5. **angular wake omission** — contact-point linear speed alone can miss a very small, rapidly rotating Kinematic support. Wake checks now also use `AngularSpeedLimit`.
6. **invalid Step diagnostics mutation** — diagnostics are reset only after `DeltaSeconds` and `SubSteps` validation, preserving the existing invalid-input state contract.
7. **apply-script ambiguous anchors** — edits use exact anchors plus brace-aware function insertion. The script aborts instead of guessing if the expected c75e7bb source shape is not present.

## Focused execution results in this environment

The focused project uses the real Stage-1 JobSystem implementation plus the Stage-2 BroadPhase/Island implementation.

- GCC Debug + `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`: **11/11 pass**
- GCC Release: **11/11 pass**
- ASan + UBSan + leak detection: **11/11 pass**, no reported issue
- GCC ThreadSanitizer: **11/11 pass**, no reported data race
- Release executable repeated 100 times: **all pass**
- Focused no-STL scan: **0 violations**

Coverage includes:

- deterministic single/multi-worker pair results
- random 2D broad phase vs brute-force AABB reference
- random 3D broad phase vs brute-force AABB reference
- ContactSlop near pairs
- same-body and non-Dynamic pair filtering
- 5,000 sparse colliders
- 256 fully dense colliders and unique stable pair ordering
- invalid bounds/margin rejection
- deterministic island roots
- Dynamic islands separated across a shared Static body
- 10,000-body Dynamic island chain
- out-of-range edge rejection

## Important verification boundary

This environment cannot clone the live GitHub repository over git networking, and the installed GitHub integration currently returns HTTP 403 for content/blob writes. Therefore I could not create a real commit on GitHub or run the full c75e7bb repository build after applying the integration edits here.

The Stage-2 core itself was compiled and sanitizer-tested. The c75e7bb integration is supplied as a strict apply script that edits the exact source shapes inspected through the GitHub connector and aborts on anchor mismatch. **Windows/MSVC, the real DxLib SDK, all existing repository tests, and the integrated 1-worker-vs-N-worker world tests still must be run after applying the bundle to the actual checkout.**

Do not report the focused 11/11 result as the repository's total test count.

## Remaining multithreading work after Stage 2

- Application-owned shared JobSystem and explicit shutdown ordering
- async CPU-side asset loading/decode with main-thread native-handle commit
- worker-local render command generation with deterministic merge
- optional task graph / dependency API for user jobs
- Physics CCD TOI iteration parallel strategy (currently kept ordered)
- island-wide sleep timer/eligibility instead of the current per-body timer logic
- feature-level contact-cache lifetime fix from the previous review
- event/trigger buffering suitable for worker production and main-thread delivery
- performance benchmarks on the target Windows machine
