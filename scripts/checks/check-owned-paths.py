#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
PATTERNS = [
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
    if not base.exists():
        continue

    for path in base.rglob("*.cpp"):
        rel = str(path.relative_to(ROOT))
        if rel in ALLOWED_FILES:
            continue

        checked += 1
        lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
        for no, line in enumerate(lines, start=1):
            if "safe_delete" in line:
                continue
            if any(pattern.search(line) for pattern in PATTERNS):
                violations.append(f"{rel}:{no}: deletion must route through platform safe-delete API")

if violations:
    print("owned-path violations:", file=sys.stderr)
    for item in violations:
        print(f"  - {item}", file=sys.stderr)
    sys.exit(1)

print(f"owned paths ok ({checked} files checked)")