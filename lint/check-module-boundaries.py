#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')
MODULES = {
    "app": {"runtime", "model", "config", "platform", "engine"},
    "runtime": {"engine", "config", "model", "platform"},
    "engine": {"config", "platform"},
    "model": {"config", "platform"},
    "config": {"platform"},
}

violations: list[str] = []
checked = 0

if SRC.exists():
    for path in list(SRC.rglob("*.hpp")) + list(SRC.rglob("*.cpp")):
        rel = path.relative_to(SRC)
        owner = rel.parts[0] if len(rel.parts) > 1 else None
        if owner not in MODULES:
            continue

        checked += 1
        lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
        for no, line in enumerate(lines, start=1):
            match = INCLUDE.match(line)
            if not match:
                continue

            target = match.group(1).split("/", 1)[0]
            if target in MODULES and target != owner and target not in MODULES[owner]:
                violations.append(f"{path.relative_to(ROOT)}:{no}: {owner} may not include {target}")

if violations:
    print("module-boundary violations:", file=sys.stderr)
    for item in violations:
        print(f"  - {item}", file=sys.stderr)
    sys.exit(1)

print(f"module boundaries ok ({checked} files checked)")