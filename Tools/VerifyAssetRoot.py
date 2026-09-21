"""Drive dxf_asset_probe across launch and layout cases.

Each case copies the probe next to a prepared tree and checks the resolved
root and the exit code. The probe uses the same entry code as the examples.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


class Failure(Exception):
    pass


def run_probe(probe: Path, settings: str, cwd: Path) -> tuple[int, str, str]:
    try:
        completed = subprocess.run(
            [str(probe), settings], cwd=str(cwd), stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, timeout=30)
    except subprocess.TimeoutExpired as error:
        raise Failure(f"probe timed out in {cwd}") from error
    return (completed.returncode,
            completed.stdout.decode("utf-8", errors="replace").strip(),
            completed.stderr.decode("utf-8", errors="replace").strip())


def norm(path: str) -> str:
    return os.path.normcase(os.path.normpath(path))


class Matrix:
    def __init__(self, probe: Path, work: Path) -> None:
        self.probe = probe
        self.work = work
        self.passed = 0

    def check(self, name: str, condition: bool, detail: str = "") -> None:
        if not condition:
            raise Failure(f"{name}: {detail}")
        print(f"PASS {name}", flush=True)
        self.passed += 1

    def make_tree(self, name: str, settings: bytes | None, assets: bytes | None) -> tuple[Path, Path]:
        root = self.work / name
        exedir = root / "Build" / "Debug"
        exedir.mkdir(parents=True, exist_ok=True)
        exe = exedir / self.probe.name
        shutil.copy(self.probe, exe)
        if settings is not None:
            (exedir / "probe.dxfpaths").write_bytes(settings)
        if assets is not None:
            assetdir = root / "Assets"
            assetdir.mkdir(exist_ok=True)
            (assetdir / "player.bmp").write_bytes(assets)
        return root, exe

    def resolve(self, exe: Path, cwd: Path) -> tuple[int, str]:
        code, out, _ = run_probe(exe, "probe.dxfpaths", cwd)
        return code, out

    def valid_settings(self) -> bytes:
        return b"Version=1\nMode=Development\nProjectRootRelative=../../\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--keep", type=Path, default=None)
    args = parser.parse_args()
    if args.keep is not None:
        work = args.keep
        work.mkdir(parents=True, exist_ok=True)
    else:
        work = Path(tempfile.mkdtemp(prefix="dxf_root_launch_"))
    matrix = Matrix(args.probe.resolve(), work)
    try:
        elsewhere = work / "elsewhere"
        elsewhere.mkdir(exist_ok=True)
        # Development tree resolves the sln-side original from several CWDs.
        root, exe = matrix.make_tree("dev", matrix.valid_settings(), b"ORIGINAL")
        for cwd in (root, exe.parent, elsewhere):
            code, out = matrix.resolve(exe, cwd)
            matrix.check(f"dev cwd={cwd.name}", code == 0 and norm(out) == norm(str(root)), out)
        # A stale exe-side copy never wins over the original.
        stale = root / "Build" / "Debug" / "Assets"
        stale.mkdir(exist_ok=True)
        (stale / "player.bmp").write_bytes(b"STALE")
        code, out = matrix.resolve(exe, elsewhere)
        matrix.check("conflict prefers original", code == 0 and norm(out) == norm(str(root)), out)
        # A missing original resolves to the missing path instead of the stale copy.
        (root / "Assets" / "player.bmp").unlink()
        code, out = matrix.resolve(exe, elsewhere)
        resolved = Path(out) / "Assets" / "player.bmp" if code == 0 else None
        matrix.check("missing original has no fallback",
                     code == 0 and norm(out) == norm(str(root)) and
                     resolved is not None and not resolved.exists(), out)
        # Packaged layout without a sidecar uses the exe directory.
        packaged, packaged_exe = matrix.make_tree("packaged", None, b"DATA")
        code, out = matrix.resolve(packaged_exe, elsewhere)
        matrix.check("packaged uses exe dir",
                     code == 0 and norm(out) == norm(str(packaged_exe.parent)), out)
        # Broken settings never fall back silently.
        broken = {
            "empty": b"",
            "bad-version": b"Version=2\nMode=Development\nProjectRootRelative=../../\n",
            "dup-key": b"Version=1\nVersion=1\nMode=Development\nProjectRootRelative=../../\n",
            "missing-key": b"Version=1\nMode=Development\n",
            "bad-utf8": b"Version=1\nMode=Develop\xffment\nProjectRootRelative=../../\n",
            "oversize": b"Version=1\nMode=Development\nProjectRootRelative=../../\n" + b"#" * 5000,
            "padded": b"Version=1\nMode=Development\nProjectRootRelative=../../\n" + b"#" * 5000,
            "missing-root": b"Version=1\nMode=Development\nProjectRootRelative=no-such-dir\n",
        }
        for name, content in broken.items():
            bad_root, bad_exe = matrix.make_tree(f"broken-{name}", content, None)
            code, out = matrix.resolve(bad_exe, elsewhere)
            matrix.check(f"broken {name} fails loudly", code != 0, f"exit={code} out={out}")
        # A moved folder keeps working through the relative settings.
        moved = work / "moved"
        shutil.copytree(root, moved)
        moved_exe = moved / "Build" / "Debug" / args.probe.name
        code, out = matrix.resolve(moved_exe, elsewhere)
        matrix.check("moved folder uses new root", code == 0 and norm(out) == norm(str(moved)), out)
        # Japanese and space names follow the same rules.
        jp_root, jp_exe = matrix.make_tree("日本語 spaced", matrix.valid_settings(), b"JP")
        code, out = matrix.resolve(jp_exe, elsewhere)
        matrix.check("japanese path resolves", code == 0 and norm(out) == norm(str(jp_root)), out)
        # A locked sidecar reports an I/O error on Windows.
        if os.name == "nt":
            import ctypes
            locked_root, locked_exe = matrix.make_tree("locked", matrix.valid_settings(), None)
            sidecar = str(locked_exe.parent / "probe.dxfpaths")
            handle = ctypes.windll.kernel32.CreateFileW(
                sidecar, 0x80000000, 0, None, 3, 0x80, None)
            try:
                if handle == -1:
                    raise Failure("could not lock the sidecar for the test")
                code, _ = matrix.resolve(locked_exe, elsewhere)
                matrix.check("locked sidecar fails loudly", code != 0, f"exit={code}")
            finally:
                ctypes.windll.kernel32.CloseHandle(handle)
        print(f"RESULT {matrix.passed} launch cases passed", flush=True)
        return 0
    except Failure as error:
        print(f"FAIL {error}", flush=True)
        return 1


if __name__ == "__main__":
    sys.exit(main())
