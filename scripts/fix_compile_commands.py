"""
PlatformIO compiledb bug workaround: src/ files are never written.
Run after 'pio run -e robot -t compiledb' to add the missing entries.
Called automatically by extra_scripts in platformio.ini.
"""
import json, os, re

# When run via extra_scripts, `env` is a SCons env injected by PlatformIO.
# When run directly as a script, fall back to cwd.
try:
    ROOT = env.subst("$PROJECT_DIR")  # noqa: F821 – SCons injects `env`
except NameError:
    ROOT = os.getcwd()

DB   = os.path.join(ROOT, "compile_commands.json")
SRC  = os.path.join(ROOT, "src")

if not os.path.exists(DB):
    print("[fix_compile_commands] compile_commands.json not found, skipping")
    exit(0)

data = json.load(open(DB))

# Use TaraCore.cpp entry as template (same flags, just swap file/output)
template = next((e for e in data if "TaraCore.cpp" in e["file"]), None)
if not template:
    print("[fix_compile_commands] no template entry found, skipping")
    exit(0)

def abspath(f):
    return f if os.path.isabs(f) else os.path.join(ROOT, f)

existing = {abspath(e["file"]) for e in data}
added = 0

for fname in os.listdir(SRC):
    if not fname.endswith((".cpp", ".c")):
        continue
    fpath = os.path.join(SRC, fname)  # already absolute
    if fpath in existing:
        continue

    rel   = os.path.relpath(fpath, ROOT)
    out   = re.sub(r"^src/", ".pio/build/robot/src/", rel).replace(".cpp", ".cpp.o").replace(".c", ".c.o")
    cmd   = re.sub(r"-o \S+", f"-o {out}", template["command"])
    cmd   = re.sub(r"\S+TaraCore\.cpp$", fpath, cmd)

    data.append({"command": cmd, "directory": ROOT, "file": fpath, "output": os.path.join(ROOT, out)})
    added += 1

json.dump(data, open(DB, "w"), indent=2)
print(f"[fix_compile_commands] added {added} src/ file(s) to compile_commands.json")
