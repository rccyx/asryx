#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

DELETE_CALLS = [
    re.compile(r"\bstd::filesystem::remove\s*\("),
    re.compile(r"\bstd::filesystem::remove_all\s*\("),
    re.compile(r"\bremove\s*\("),
    re.compile(r"\bunlink\s*\("),
    re.compile(r"\brmdir\s*\("),
]

ALLOWED_FILES = {
    "src/platform/fs.cpp",
    "tests/test_main.cpp",
}

violations: list[str] = []
checked = 0

for base in (ROOT / "src", ROOT / "tests"):
    for path in base.rglob("*.cpp"):
        rel = str(path.relative_to(ROOT))

        if rel in ALLOWED_FILES:
            continue

        checked += 1

        for line_no, line in enumerate(path.read_text(encoding="utf-8", errors="ignore").splitlines(), 1):
            if "safe_delete" in line:
                continue

            if any(pattern.search(line) for pattern in DELETE_CALLS):
                violations.append(f"{rel}:{line_no}: deletion must use platform safe-delete API")

if checked == 0:
    print("owned-path check failed: 0 files checked", file=sys.stderr)
    sys.exit(1)

if violations:
    print("owned-path violations:", file=sys.stderr)
    for violation in violations:
        print(f"  - {violation}", file=sys.stderr)
    sys.exit(1)

print(f"owned paths ok ({checked} files checked)")