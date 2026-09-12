"""Rebuild portable tests and standalone headers. Does not verify the real DxLib SDK."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import wave
from ValidationSupport import project_version, run_logged, validation_report

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-root", type=Path, default=ROOT / "Build" / "Validation")
    parser.add_argument("--with-sanitizers", action="store_true")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    build_root = args.build_root.resolve()
    logs = ROOT / "Docs" / "Validation"
    build_root.mkdir(parents=True, exist_ok=True)
    logs.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment.update(ASAN_OPTIONS="detect_leaks=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    summary: dict[str, object] = {"native_sdk_verified": False, "warnings_as_errors": True, "profiles": {}}

    def run(name: str, command: list[str]) -> str:
        return run_logged(logs, name, command, cwd=ROOT, environment=environment)

    with validation_report(logs, summary):
        summary["version"] = project_version(ROOT)
        profiles = [("debug", "Debug", []), ("release", "Release", [])]
        if args.with_sanitizers:
            if os.name == "nt" or not shutil.which("clang++"):
                raise RuntimeError("The sanitizer profile requires Linux and clang++")
            profiles.append(("sanitized", "Debug", ["-DCMAKE_CXX_COMPILER=clang++", "-DDXF_SANITIZERS=ON"]))
        suffix = ".exe" if os.name == "nt" else ""
        for name, configuration, extra in profiles:
            build = build_root / name
            run(f"{name}-configure", ["cmake", "-S", str(ROOT), "-B", str(build), "-G", "Ninja",
                f"-DCMAKE_BUILD_TYPE={configuration}", "-DDXF_BUILD_TESTS=ON", "-DDXF_BUILD_NATIVE=OFF",
                "-DDXF_BUILD_EXAMPLE=OFF", "-DDXF_BUILD_NATIVE_SMOKE=OFF", "-DDXF_WARNINGS_AS_ERRORS=ON", *extra])
            run(f"{name}-build", ["cmake", "--build", str(build), "--parallel", str(args.jobs)])
            run(f"{name}-ctest", ["ctest", "--test-dir", str(build), "--output-on-failure"])
            counts = []
            for kind, binary in [("framework", "dxf_tests"), ("native", "dxf_native_contract_tests")]:
                output = run(f"{name}-cases-{kind}", [str(build / (binary + suffix))])
                match = re.search(r"(\d+)/(\d+) passed", output)
                if not match or match.group(1) != match.group(2):
                    raise RuntimeError("Missing or inconsistent per-case test count")
                counts.append(int(match.group(1)))
            summary["profiles"][name] = {"framework_cases": counts[0], "native_contract_cases": counts[1]}

        consumer = build_root / "ConsumerSource"
        consumer.mkdir(exist_ok=True)
        public_roots = sorted((ROOT / "Source").glob("*/Public"))
        headers = [(base, path) for base in public_roots for path in sorted(base.rglob("*.h"))]
        sources = []
        for index, (base, path) in enumerate(headers):
            unit = f"Header_{index:03}.cpp"
            (consumer / unit).write_text(f'#include "{path.relative_to(base).as_posix()}"\n', encoding="utf-8")
            sources.append(unit)
        (consumer / "SandboxHeader.cpp").write_text('#include "SandboxGame.h"\n', encoding="utf-8")
        sources.append("SandboxHeader.cpp")
        (consumer / "Main.cpp").write_text('''#include "Dxf/SlotMap.h"
#include "Dxf/Clock.h"
class DProbe final : public Dxf::DObject {};
int main()
{
    Dxf::TSlotMap<Dxf::DObject> Storage;
    auto Handle = Storage.Insert(std::make_unique<DProbe>());
    Dxf::FFrameClock Clock;
    return Handle.Get() && Clock.Sample(0.0) ? 0 : 1;
}
''', encoding="utf-8")
        (consumer / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.24)
project(DxfConsumer LANGUAGES CXX)
add_subdirectory("{ROOT.as_posix()}" framework)
if(DXF_BUILD_TESTS OR DXF_BUILD_NATIVE OR DXF_BUILD_EXAMPLE OR DXF_BUILD_NATIVE_SMOKE OR DXF_INSTALL)
    message(FATAL_ERROR "Subproject defaults must not enable tests, samples or native dependencies")
endif()
add_library(HeaderChecks OBJECT {' '.join(sources)})
target_link_libraries(HeaderChecks PRIVATE dxf::framework)
target_include_directories(HeaderChecks PRIVATE "{(ROOT/'Source/Native/Public').as_posix()}" "{(ROOT/'Examples/Sandbox').as_posix()}")
dxf_warnings(HeaderChecks)
add_executable(Consumer Main.cpp)
target_link_libraries(Consumer PRIVATE dxf::framework)
''', encoding="utf-8")
        consumer_build = build_root / "ConsumerBuild"
        run("consumer-configure", ["cmake", "-S", str(consumer), "-B", str(consumer_build), "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug", "-DDXF_WARNINGS_AS_ERRORS=ON"])
        run("consumer-build", ["cmake", "--build", str(consumer_build), "--parallel", str(args.jobs)])
        run("consumer-run", [str(consumer_build / ("Consumer" + suffix))])
        summary["standalone_public_headers"] = len(headers)
        summary["standalone_sample_headers"] = 1
        (logs / "headers.log").write_text("\n".join("PASS " + str(path.relative_to(ROOT)) for _, path in headers)
                                          + "\nPASS Examples/Sandbox/SandboxGame.h\n", encoding="utf-8")
        bitmap = (ROOT / "Assets/player.bmp").read_bytes()
        if bitmap[:2] != b"BM" or struct.unpack_from("<I", bitmap, 2)[0] != len(bitmap):
            raise RuntimeError("Invalid sample BMP header")
        if struct.unpack_from("<iiHH", bitmap, 18) != (64, 64, 1, 24):
            raise RuntimeError("Invalid sample BMP format")
        with wave.open(str(ROOT / "Assets/confirm.wav"), "rb") as audio:
            if (audio.getnchannels(), audio.getsampwidth(), audio.getframerate()) != (1, 2, 22050) or audio.getnframes() <= 0:
                raise RuntimeError("Invalid sample WAV format")
        summary["sample_assets"] = "BMP 64x64 24-bit; WAV mono 16-bit 22050Hz"
        summary["environment"] = {"python": sys.version.split()[0], "platform": sys.platform,
                                  "cmake": run("cmake-version", ["cmake", "--version"]).splitlines()[0]}
    print(json.dumps(summary, ensure_ascii=False, indent=2), flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, subprocess.TimeoutExpired, OSError) as error:
        print(f"VALIDATION FAILED: {error}", file=sys.stderr)
        raise SystemExit(1)
