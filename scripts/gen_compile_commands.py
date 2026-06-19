#!/usr/bin/env python3
"""
Generate compile_commands.json from a verbose PlatformIO build.
Captures ALL compiled files including src/ which compiledb misses.

Usage: python3 scripts/gen_compile_commands.py
"""
import json, os, re, subprocess, shlex

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DB   = os.path.join(ROOT, "compile_commands.json")

print("[gen] Running full clean build (verbose)...")
result = subprocess.run(
    ["pio", "run", "-e", "robot", "-v"],
    cwd=ROOT,
    capture_output=True,
    text=True,
)

output = result.stdout + result.stderr

entries = []
# Match lines that start with the cross-compiler (compile, not link)
pattern = re.compile(
    r"(xtensa-esp32-elf-g(?:cc|\+\+)\s+-o\s+\S+\.o\s+-c\s+.+)$",
    re.MULTILINE,
)

for m in pattern.finditer(output):
    raw_cmd = m.group(1).strip()

    # Extract the source file (last non-flag token)
    # PlatformIO puts it at the very end
    parts = shlex.split(raw_cmd)
    src = parts[-1]
    if not os.path.isabs(src):
        src = os.path.join(ROOT, src)

    out_match = re.search(r"-o\s+(\S+\.o)", raw_cmd)
    out = out_match.group(1) if out_match else ""
    if out and not os.path.isabs(out):
        out = os.path.join(ROOT, out)

    entries.append({
        "command":   raw_cmd,
        "directory": ROOT,
        "file":      src,
        "output":    out,
    })

# Deduplicate by file (keep last occurrence)
seen = {}
for e in entries:
    seen[e["file"]] = e
entries = list(seen.values())

json.dump(entries, open(DB, "w"), indent=2)
print(f"[gen] Wrote {len(entries)} entries to compile_commands.json")

# Verify src/ files are present
src_files = [e["file"] for e in entries if f"{ROOT}/src/" in e["file"]]
print("[gen] src/ entries:")
for f in src_files:
    print(f"  {f}")
