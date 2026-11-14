"""Source checkpoints and alternating before/after VM measurements."""
import datetime
import hashlib
import json
from pathlib import Path
import random
import re
import shutil
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
BENCH = ROOT / "misc/benchmarks"
CHECKPOINTS = ROOT / "misc/optimization"
CASES = {"integer_loop": 200000, "double_loop": 200000,
         "recursive_fib": 22, "object_methods": 200000}


def timestamp():
    return datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=9))).strftime("%Y%m%d-%H%M%S-%f")


def checkpoint(name):
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", name):
        raise ValueError("Checkpoint name may only contain letters, digits, underscores and hyphens")
    return CHECKPOINTS / name


def source_files():
    return sorted(path for path in (ROOT / "Ocarina").rglob("*")
                  if path.is_file() and "dependencies" not in path.parts
                  and path.suffix in (".cpp", ".h", ".vcxproj", ".filters", ".vert", ".frag")) + [ROOT / "README.md", ROOT / "Ocarina.slnx"]


def snapshot(name):
    destination = checkpoint(name)
    if destination.exists():
        raise RuntimeError(f"Checkpoint already exists: {destination}")
    destination.mkdir(parents=True)
    files = {}
    for path in source_files():
        relative = path.relative_to(ROOT)
        copied = destination / "source" / relative
        copied.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, copied)
        files[relative.as_posix()] = hashlib.sha256(copied.read_bytes()).hexdigest()
    (destination / "manifest.json").write_text(json.dumps(dict(created=timestamp(), files=files), indent=2), encoding="utf-8")
    shutil.copy2(ROOT / "bench.py", destination / "reference-bench.py")
    for path in BENCH.iterdir():
        if path.is_file() and path.suffix in (".oc", ".cpp", ".h"):
            copied = destination / "benchmarks" / path.name
            copied.parent.mkdir(exist_ok=True)
            shutil.copy2(path, copied)
    if (BENCH / "results/results.json").exists():
        shutil.copy2(BENCH / "results/results.json", destination / "previous-results.json")
    print(f"Saved source checkpoint: {destination}", flush=True)
    return destination


def capture_binary(name):
    destination = checkpoint(name)
    manifest = json.loads((destination / "manifest.json").read_text(encoding="utf-8"))
    if {path.relative_to(ROOT).as_posix() for path in source_files()} != set(manifest["files"]):
        raise RuntimeError("Source file inventory changed since checkpoint")
    for relative, expected in manifest["files"].items():
        if hashlib.sha256((ROOT / relative).read_bytes()).hexdigest() != expected:
            raise RuntimeError(f"Source changed since checkpoint: {relative}")
    binary = destination / "bin"
    binary.mkdir(exist_ok=True)
    for filename in ("ocarina_bench.exe", "glew32.dll", "SDL3.dll", "SDL3_ttf.dll", "SDL3_mixer.dll"):
        shutil.copy2(BENCH / "build" / filename, binary / filename)
    hashes = {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in binary.iterdir() if path.is_file()}
    (destination / "binaries.json").write_text(json.dumps(hashes, indent=2), encoding="utf-8")
    print(f"Saved benchmark executable: {binary}", flush=True)


def restore(name):
    destination = checkpoint(name)
    manifest = json.loads((destination / "manifest.json").read_text(encoding="utf-8"))
    for relative, expected in manifest["files"].items():
        saved = (destination / "source" / relative).resolve()
        if not saved.is_relative_to((destination / "source").resolve()) or not (ROOT / relative).resolve().is_relative_to(ROOT):
            raise RuntimeError(f"Invalid checkpoint path: {relative}")
        if hashlib.sha256(saved.read_bytes()).hexdigest() != expected:
            raise RuntimeError(f"Damaged checkpoint file: {relative}")
    rescue = f"restore-save-{timestamp()}"
    snapshot(rescue)
    rescue_manifest = json.loads((CHECKPOINTS / rescue / "manifest.json").read_text(encoding="utf-8"))
    for relative in rescue_manifest["files"].keys() - manifest["files"].keys():
        target = (ROOT / relative).resolve()
        if not target.is_relative_to((ROOT / "Ocarina").resolve()):
            raise RuntimeError(f"Invalid added source path: {relative}")
        target.unlink()
    for relative in manifest["files"]:
        target = (ROOT / relative).resolve()
        if not target.is_relative_to(ROOT):
            raise RuntimeError(f"Invalid restore target: {relative}")
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(destination / "source" / relative, target)
    print(f"Restored {name}. Previous working source is preserved in {rescue}.", flush=True)


def compare(version="before"):
    sys.path.insert(0, str(ROOT))
    import bench
    affinity_mask = bench.pin_cpu()
    baseline_name = version
    before = checkpoint(version) / "bin/ocarina_bench.exe"
    after = BENCH / "build/ocarina_bench.exe"
    if not before.exists() or not after.exists():
        raise RuntimeError("Build current VM and capture the before checkpoint executable first")
    rows = []
    order = [(case, size, round_index) for round_index in range(3) for case, size in CASES.items()]
    random.Random(20261113).shuffle(order)
    for case, size, round_index in order:
        versions = [("before", before), ("current", after)]
        if round_index % 2:
            versions.reverse()
        for version, executable in versions:
            print(f"Comparing {case}: {version}, round {round_index + 1}/3...", flush=True)
            output = bench.command([executable, BENCH / f"{case}.oc", case, size], timeout=180)
            row = json.loads(output)
            row.update(version=version, round=round_index + 1)
            rows.append(row)
    summary = []
    for case in CASES:
        matching = [row for row in rows if row["workload"] == case]
        if len({row["checksum"] for row in matching}) != 1:
            raise RuntimeError(f"Before/after checksum mismatch: {case}")
        times = {version: statistics.median([sample for row in matching if row["version"] == version
                                            for sample in row["samples_ms"]]) for version in ("before", "current")}
        summary.append(dict(workload=case, before_ms=times["before"], current_ms=times["current"],
                            speedup=times["before"] / times["current"]))
    environment = bench.metadata()
    environment["affinity_mask"] = affinity_mask
    result = dict(measured=timestamp(), baseline=baseline_name, rounds=3, samples_per_version=27,
                  executables=dict(before_sha256=hashlib.sha256(before.read_bytes()).hexdigest(),
                                   current_sha256=hashlib.sha256(after.read_bytes()).hexdigest()),
                  environment=environment, summary=summary, measurements=rows)
    CHECKPOINTS.mkdir(exist_ok=True)
    (CHECKPOINTS / "comparison.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"\n{'Workload':<20}{'Before (ms)':>14}{'Current (ms)':>15}{'Speedup':>12}")
    for row in summary:
        print(f"{row['workload']:<20}{row['before_ms']:>14.3f}{row['current_ms']:>15.3f}{row['speedup']:>11.2f}x")
    return result


def verify():
    sys.path.insert(0, str(ROOT))
    import bench
    directory = BENCH / "build/checks"
    directory.mkdir(parents=True, exist_ok=True)
    devcmd = bench.visual_studio() / "Common7/Tools/VsDevCmd.bat"
    response = directory / "numeric.rsp"
    response.write_text(" ".join([
        "/nologo /std:c++20 /O2 /MD /EHsc /utf-8 /DNDEBUG /fp:precise",
        f'/I"{ROOT / "Ocarina"}"', f'"{CHECKPOINTS / "checks/numeric.cpp"}"',
        f'"{ROOT / "Ocarina/vm_numeric.cpp"}"', f'/Fo"{directory.as_posix()}/"',
        f'/Fe"{directory / "numeric_checks.exe"}"', "/link /DEBUG:NONE"
    ]), encoding="utf-8")
    script = directory / "numeric.cmd"
    script.write_text(f'@echo off\ncall "{devcmd}" -no_logo -arch=x64 -host_arch=x64\n'
                      f'if errorlevel 1 exit /b 1\ncl.exe @"{response}"\nexit /b %errorlevel%\n', encoding="utf-8")
    bench.command(["cmd.exe", "/d", "/c", script], timeout=120)
    numeric = bench.command([directory / "numeric_checks.exe"], timeout=60)
    print(numeric, flush=True)
    checks = [dict(name="numeric", result=numeric)]
    before = CHECKPOINTS / "before/bin/ocarina_bench.exe"
    current = BENCH / "build/ocarina_bench.exe"
    for case, versions in (("core", [("before", before), ("current", current)]),
                           ("increment", [("current", current)])):
        for version, executable in versions:
            output = bench.command([executable, CHECKPOINTS / f"checks/{case}.oc", case, "1"], timeout=60)
            row = json.loads(output)
            if row["checksum"] != 0:
                raise RuntimeError(f"{case} failed on {version}: {row}")
            print(f"{case}: {version} passed", flush=True)
            checks.append(dict(name=case, version=version, checksum=row["checksum"]))
    for case, message in (("divide_zero", "Division by zero"), ("call_overflow", "VM call stack overflow"),
                          ("missing_return", "Function did not return a value")):
        for version, executable in (("before", before), ("current", current)):
            result = subprocess.run([str(executable), str(CHECKPOINTS / f"checks/{case}.oc"), case, "1"],
                                    cwd=ROOT, capture_output=True, text=True, timeout=60)
            if result.returncode != 1 or message not in result.stderr:
                raise RuntimeError(f"Expected {message} on {version}: {result.stdout} {result.stderr}")
            print(f"{case}: {version} correctly rejected", flush=True)
            checks.append(dict(name=case, version=version, expected_error=message))
    (CHECKPOINTS / "verification.json").write_text(json.dumps(dict(verified=timestamp(), checks=checks), indent=2), encoding="utf-8")
    return checks
