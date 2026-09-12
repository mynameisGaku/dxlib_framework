"""Reject STL dependencies in framework, examples, and C++ tests."""
from __future__ import annotations

import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CPP_SUFFIXES = {".h", ".hpp", ".inl", ".cpp", ".cc", ".cxx"}
STANDARD_HEADERS = set("""algorithm any array atomic barrier bit bitset charconv chrono codecvt
compare complex concepts condition_variable coroutine deque exception execution expected filesystem
flat_map flat_set format forward_list fstream functional future generator hazard_pointer hive initializer_list
iomanip ios iosfwd iostream istream iterator latch limits list locale map mdspan memory memory_resource
mutex new numbers numeric optional ostream print queue random ranges ratio rcu regex scoped_allocator
semaphore set shared_mutex source_location span sstream stack stacktrace stdexcept stop_token streambuf
string string_view strstream syncstream system_error text_encoding thread tuple type_traits typeindex
typeinfo unordered_map unordered_set utility valarray variant vector version cassert cctype cerrno cfenv
cfloat cinttypes ciso646 climits clocale cmath csetjmp csignal cstdalign cstdarg cstdbool cstddef cstdint
cstdio cstdlib cstring ctgmath ctime cuchar cwchar cwctype""".split())
LANGUAGE_HEADERS = {"new", "initializer_list"}
LANGUAGE_SYMBOLS = {"align_val_t", "initializer_list"}
TOKEN = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\([\s\S]*?\)(?P=delimiter)"|"(?:\\.|[^"\\])*"|(?<![\w])\'(?:\\.|[^\'\\\r\n])*\'')


def scan_text(path: Path, source: str) -> list[str]:
    """Return diagnostics while ignoring comments and string/character contents."""
    toolbox = "Toolbox" in path.parts and "Source" in path.parts
    tokens = list(TOKEN.finditer(source))
    # Preserve source positions so diagnostics and preprocessor checks share real line numbers.
    code = TOKEN.sub(lambda match: re.sub(r'[^\n]', ' ', match.group()), source)
    findings = []
    for match in re.finditer(r'^[ \t]*(#\s*include\s*[<"]([^>"\n]+)[>"])', source, re.MULTILINE):
        if any(token.start() <= match.start(1) < token.end() for token in tokens):
            continue
        header = match.group(2)
        if header in STANDARD_HEADERS and not (toolbox and header in LANGUAGE_HEADERS):
            line = source.count("\n", 0, match.start()) + 1
            findings.append(f"{path}:{line}: STL header <{header}> is forbidden; implement it in Toolbox")
    for match in re.finditer(r'\bstd\s*::\s*(\w+)|\busing\s+namespace\s+std\b|\bnamespace\s+\w+\s*=\s*std\b', code):
        if toolbox and match.group(1) in LANGUAGE_SYMBOLS:
            continue
        line = code.count("\n", 0, match.start()) + 1
        findings.append(f"{path}:{line}: STL namespace use is forbidden: {match.group()}")
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args()
    findings = []
    count = 0
    for directory in ("Source", "Examples", "Tests"):
        for path in sorted((args.root / directory).rglob("*")):
            if path.is_file() and path.suffix.lower() in CPP_SUFFIXES:
                count += 1
                findings.extend(scan_text(path.relative_to(args.root), path.read_text(encoding="utf-8-sig")))
    if not count:
        print("No C++ source files found; check --root")
        return 1
    for finding in findings:
        print(finding)
    print(f"No-STL check: {count} files, {len(findings)} violations")
    return 1 if findings else 0


if __name__ == "__main__":
    raise SystemExit(main())
