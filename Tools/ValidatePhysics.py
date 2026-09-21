"""Validate the focused collision/CCD/fixed-step suite, not the complete framework."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    output = (args.output or root / "Build" / "PhysicsValidation").resolve()
    output.mkdir(parents=True, exist_ok=True)
    summary_path = output / "Summary.json"
    summary: dict = {
        "status": "running",
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "focused collision, CCD and fixed-step tests; not the full framework or DxLib",
        "commands": [],
        "profiles": [],
    }

    def write_summary() -> None:
        summary_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    def run(name: str, command: list[str], env: dict[str, str] | None = None) -> str:
        log = output / f"{name}.log"
        entry = {"name": name, "command": command, "log": log.name}
        summary["commands"].append(entry)
        write_summary()
        try:
            result = subprocess.run(command, cwd=root, text=True, encoding="utf-8", errors="replace",
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180, env=env)
            text = result.stdout
            entry["exit_code"] = result.returncode
        except subprocess.TimeoutExpired as error:
            raw = error.stdout or b""
            text = raw.decode("utf-8", errors="replace") if isinstance(raw, bytes) else raw
            entry["exit_code"] = "timeout"
        log.write_text("COMMAND " + json.dumps(command) + "\n" + text +
                       "\nEXIT " + str(entry["exit_code"]) + "\n", encoding="utf-8")
        write_summary()
        if entry["exit_code"] != 0:
            raise RuntimeError(f"{name} failed; see {log}")
        print(f"PASS {name}", flush=True)
        return text

    write_summary()
    try:
        for tool in ("cmake", "ctest", "g++", "clang++"):
            if shutil.which(tool) is None:
                raise RuntimeError(f"Required tool not found: {tool}. No profile was silently skipped.")
            run(f"version-{tool.replace('+', 'p')}", [tool, "--version"])
        profiles = (("gcc-debug", "g++", "Debug", False),
                    ("gcc-release", "g++", "Release", False),
                    ("clang-release", "clang++", "Release", False),
                    ("clang-sanitized", "clang++", "Debug", True))
        for name, compiler, configuration, sanitized in profiles:
            build = output / name
            # The standalone entry configures the repository root itself so the
            # same dxf::physics body backs every physics test binary.
            command = ["cmake", "-S", str(root), "-B", str(build),
                       f"-DCMAKE_CXX_COMPILER={shutil.which(compiler)}", f"-DCMAKE_BUILD_TYPE={configuration}",
                       "-DDXF_BUILD_NATIVE=OFF", "-DDXF_BUILD_TESTS=ON",
                       f"-DDXF_SANITIZERS={'ON' if sanitized else 'OFF'}"]
            run(name + "-configure", command)
            run(name + "-build", ["cmake", "--build", str(build), "--parallel", "4"])
            env = os.environ.copy()
            if sanitized:
                env.update(ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
            run(name + "-ctest", ["ctest", "--test-dir", str(build), "-R", "PhysicsContinuation",
                                  "--output-on-failure"], env)
            text = run(name + "-cases", [str(build / "dxf_physics_tests")], env)
            totals = [(int(a), int(b)) for a, b in re.findall(r"RESULT (\d+)/(\d+) passed", text)]
            if not totals or any(a != b for a, b in totals):
                raise RuntimeError(f"Missing or failing case summary in {name}")
            summary["profiles"].append({"name": name, "passed": sum(a for a, _ in totals),
                                        "total": sum(b for _, b in totals), "sanitizers": sanitized})
            write_summary()
        headers = sorted((root / "Source" / "Toolbox" / "Public").rglob("*.h"))
        for header in headers:
            translation_unit = output / ("header-" + header.stem + ".cpp")
            translation_unit.write_text('#include "' + header.relative_to(root / "Source" / "Toolbox" / "Public").as_posix() + '"\n', encoding="utf-8")
            run("header-" + header.stem, ["clang++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-fsyntax-only",
                                         "-I", str(root / "Source" / "Toolbox" / "Public"), str(translation_unit)])
        summary["standalone_headers"] = len(headers)
        run("no-stl", [sys.executable, str(root / "Tools" / "CheckNoStl.py"), "--root", str(root)])
        summary["status"] = "passed"
    except Exception as error:
        summary["status"] = "failed"
        summary["error"] = str(error)
        print(str(error), file=sys.stderr)
    finally:
        summary["finished_utc"] = datetime.now(timezone.utc).isoformat()
        write_summary()
    return 0 if summary["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
