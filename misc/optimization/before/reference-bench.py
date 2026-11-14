"""Run without arguments to build, benchmark, and print the comparison tables."""
import argparse
import ctypes
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import shutil
import statistics
import subprocess
import sys
import winreg

ROOT = Path(__file__).resolve().parent
HERE = ROOT / "misc" / "benchmarks"
BUILD = HERE / "build"
RESULTS = HERE / "results"
CASES = {"integer_loop": 200000, "double_loop": 200000,
         "recursive_fib": 22, "object_methods": 200000, "baseline": 1}


def command(args, **kwargs):
    result = subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True,
                            text=True, encoding="utf-8", errors="replace", **kwargs)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {args}\n{result.stdout}\n{result.stderr}")
    return result.stdout.strip()


def visual_studio():
    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    return Path(command([vswhere, "-latest", "-products", "*", "-property", "installationPath"]))


def build():
    BUILD.mkdir(exist_ok=True)
    print("[1/3] Building current Ocarina Release VM objects...", flush=True)
    vs = visual_studio()
    devcmd = vs / "Common7/Tools/VsDevCmd.bat"
    msbuild = vs / "MSBuild/Current/Bin/MSBuild.exe"
    objects_dir = BUILD / "obj"
    compile_script = BUILD / "compile_objects.cmd"
    compile_script.write_text(
        f'@echo off\ncall "{devcmd}" -no_logo -arch=x64 -host_arch=x64\n'
        'if errorlevel 1 exit /b 1\n'
        f'"{msbuild}" "{ROOT / "Ocarina/Ocarina.vcxproj"}" /t:ClCompile '
        '/p:Configuration=Release /p:Platform=x64 '
        f'/p:OutDir="{BUILD.as_posix()}/" /p:IntDir="{objects_dir.as_posix()}/" /m:4 /nologo /v:minimal\n'
        'exit /b %errorlevel%\n', encoding="utf-8")
    try:
        output = command(["cmd.exe", "/d", "/c", compile_script], timeout=240)
        (BUILD / "objects-build.log").write_text(output, encoding="utf-8")
    except Exception as error:
        (BUILD / "objects-build.log").write_text(str(error), encoding="utf-8")
        raise
    includes = [ROOT / "Ocarina", ROOT / "Ocarina/dependencies/include"]
    includes += [ROOT / "Ocarina/engine" / name for name in ("core", "renderer", "model", "ui", "util", "error")]
    common = ["/nologo", "/std:c++20", "/O2", "/GL", "/MD", "/EHsc", "/utf-8", "/DNDEBUG", "/fp:precise"]
    objects = [path for path in objects_dir.glob("*.obj") if path.name not in ("main.obj", "shell_commands.obj")]
    if len(objects) < 20:
        raise RuntimeError("Release VM objects are missing")
    runner = common + ["/DSDL_MAIN_HANDLED"] + [f'/I"{path}"' for path in includes]
    runner += [f'"{HERE / "ocarina_runner.cpp"}"', f'/Fo"{BUILD / "ocarina_runner.obj"}"',
               f'/Fe"{BUILD / "ocarina_bench.exe"}"']
    runner += [f'"{path}"' for path in objects]
    runner += ["/link", "/LTCG", "/DEBUG:NONE", f'/LIBPATH:"{ROOT / "Ocarina/dependencies/lib"}"',
               "opengl32.lib", "glew32.lib", "SDL3.lib", "SDL3_ttf.lib", "SDL3_mixer.lib"]
    native = common + [f'"{HERE / "native.cpp"}"', f'/Fo"{BUILD / "native.obj"}"',
                       f'/Fe"{BUILD / "native_bench.exe"}"', "/link", "/LTCG", "/DEBUG:NONE"]
    print("[2/3] Linking Ocarina and C++ benchmark programs...", flush=True)
    for name, args in (("ocarina", runner), ("native", native)):
        response = BUILD / f"{name}.rsp"
        response.write_text(" ".join(args), encoding="utf-8")
        script = BUILD / f"link_{name}.cmd"
        script.write_text(f'@echo off\ncall "{devcmd}" -no_logo -arch=x64 -host_arch=x64\n'
                          f'if errorlevel 1 exit /b 1\ncl.exe @"{response}"\nexit /b %errorlevel%\n',
                          encoding="utf-8")
        try:
            output = command(["cmd.exe", "/d", "/c", script], timeout=240)
            (BUILD / f"{name}-build.log").write_text(output, encoding="utf-8")
        except Exception as error:
            (BUILD / f"{name}-build.log").write_text(str(error), encoding="utf-8")
            raise
    for dll in ("glew32.dll", "SDL3.dll", "SDL3_ttf.dll", "SDL3_mixer.dll"):
        shutil.copy2(ROOT / "Ocarina/dependencies/lib" / dll, BUILD / dll)
    environment = os.environ.copy()
    environment.update(DOTNET_CLI_HOME=str(BUILD / "dotnet-home"),
                       NUGET_PACKAGES=str(BUILD / "nuget"), DOTNET_SKIP_FIRST_TIME_EXPERIENCE="1",
                       DOTNET_CLI_TELEMETRY_OPTOUT="1", DOTNET_NOLOGO="1")
    print("[3/3] Building C# benchmark program...", flush=True)
    output = command(["dotnet", "build", HERE / "csharp/Reference.csproj", "-c", "Release",
                      "--configfile", HERE / "csharp/NuGet.Config", "--nologo"], env=environment, timeout=180)
    (BUILD / "csharp-build.log").write_text(output, encoding="utf-8")
    print("Built current Ocarina Release VM harness, C++ reference and C# reference.", flush=True)


def pin_cpu():
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.GetCurrentProcess.restype = ctypes.c_void_p
    kernel.GetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t)]
    kernel.SetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    handle = kernel.GetCurrentProcess()
    available, system = ctypes.c_size_t(), ctypes.c_size_t()
    if kernel.GetProcessAffinityMask(handle, ctypes.byref(available), ctypes.byref(system)):
        selected = available.value & -available.value
        if kernel.SetProcessAffinityMask(handle, selected):
            return selected
    return None


def metadata():
    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\DESCRIPTION\System\CentralProcessor\0") as key:
        cpu = winreg.QueryValueEx(key, "ProcessorNameString")[0].strip()
    digest = hashlib.sha256()
    for path in sorted((ROOT / "Ocarina").rglob("*")):
        if path.suffix in (".cpp", ".h", ".vcxproj") and "dependencies" not in path.parts:
            digest.update(str(path.relative_to(ROOT)).encode())
            digest.update(path.read_bytes())
    return dict(date=datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=9))).isoformat(),
                cpu=cpu, os=platform.platform(), logical_cpus=os.cpu_count(),
                python=sys.version, python_executable=sys.executable,
                node=command(["node", "--version"]),
                dotnet=command(["dotnet", "--version"]), source_sha256=digest.hexdigest(),
                python_jit_enabled=bool(hasattr(sys, "_jit") and sys._jit.is_enabled()),
                msvc_tools=str(next((visual_studio() / "VC/Tools/MSVC").iterdir()).name),
                git_head=command(["git", "rev-parse", "HEAD"]),
                dirty_files=command(["git", "status", "--short"]).splitlines(),
                timer="steady_clock / perf_counter_ns / performance.now",
                samples=9, warmup_calls=5, minimum_batch_ms=50,
                timing_scope="Warm in-process kernel time. Excludes process startup, source compilation, bytecode loading, graphics and printing. Includes Ocarina run setup/cleanup.",
                optimizations="MSVC Release /O2 /GL /MD /fp:precise; Node default V8 JIT; CPython default without JIT; .NET Release with tiered compilation disabled",
                overflow="Integer intermediates stay inside signed int32; double kernels use IEEE binary64")


def run():
    RESULTS.mkdir(exist_ok=True)
    info = metadata()
    info["affinity_mask"] = pin_cpu()
    cells = [(language, case, size) for case, size in CASES.items()
             for language in ("Ocarina", "C++", "Python", "JavaScript", "C#")]
    random.Random(20261112).shuffle(cells)
    rows = []
    for language, case, size in cells:
        if language == "Ocarina":
            args = [BUILD / "ocarina_bench.exe", HERE / f"{case}.oc", case, size]
        elif language == "C++":
            args = [BUILD / "native_bench.exe", case, size]
        elif language == "Python":
            args = [sys.executable, "-B", HERE / "reference.py", case, size]
        elif language == "C#":
            args = ["dotnet", BUILD / "csharp/Reference.dll", case, size]
        else:
            args = ["node", HERE / "reference.js", case, size]
        print(f"Measuring {language}: {case} ({size})...", flush=True)
        raw = command(args, timeout=180)
        row = json.loads(raw)
        times = row["samples_ms"]
        row.update(median_ms=statistics.median(times), min_ms=min(times), max_ms=max(times),
                   stdev_ms=statistics.stdev(times))
        rows.append(row)
        (RESULTS / "results.json").write_text(json.dumps(dict(metadata=info, measurements=rows), indent=2), encoding="utf-8")
        print(f"  {row['median_ms']:.6f} ms; checksum={row['checksum']}", flush=True)
    for case in CASES:
        matching = [row for row in rows if row["workload"] == case]
        if len({row["checksum"] for row in matching}) != 1:
            raise RuntimeError(f"Checksum mismatch in {case}: {matching}")
    csv = ["workload,size,language,checksum,median_ms,min_ms,max_ms,stdev_ms,batch_repetitions"]
    for case in CASES:
        for row in sorted((row for row in rows if row["workload"] == case), key=lambda row: row["median_ms"]):
            csv.append(",".join(str(row[key]) for key in csv[0].split(",")))
    (RESULTS / "results.csv").write_text("\n".join(csv) + "\n", encoding="utf-8")
    summarize()
    print("All workloads produced matching checksums. Saved results.json and results.csv.", flush=True)


def print_results(values):
    languages = ["C++", "C#", "JavaScript", "Python", "Ocarina"]
    workloads = [case for case in CASES if case != "baseline"]
    print("\nBenchmark results: median milliseconds (lower is better)", flush=True)
    print(f"{'Workload':<20}" + "".join(f"{language:>13}" for language in languages))
    print("-" * 85)
    for case in workloads:
        print(f"{case:<20}" + "".join(f"{values[case, language]['median_ms']:>13.3f}" for language in languages))
    print("\nOcarina relative execution time (x; higher means slower)")
    print(f"{'Workload':<20}" + "".join(f"{language:>13}" for language in languages[:-1]))
    print("-" * 72)
    for case in workloads:
        current = values[case, "Ocarina"]["median_ms"]
        ratios = [f"{current / values[case, language]['median_ms']:.2f}x" for language in languages[:-1]]
        print(f"{case:<20}" + "".join(f"{ratio:>13}" for ratio in ratios))
    print(f"\nReport: {HERE / 'README.md'}")
    print(f"Raw samples: {RESULTS / 'results.json'}", flush=True)


def summarize():
    document = json.loads((RESULTS / "results.json").read_text(encoding="utf-8"))
    rows, info = document["measurements"], document["metadata"]
    values = {(row["workload"], row["language"]): row for row in rows}
    languages = ["C++", "C#", "JavaScript", "Python", "Ocarina"]
    labels = {"integer_loop": "정수 연산 20만 회", "double_loop": "double 연산 20만 회",
              "recursive_fib": "재귀 Fibonacci(22)", "object_methods": "객체 상태 갱신 20만 회"}
    lines = ["# Ocarina 로컬 언어 벤치마크", "", f"측정: {info['date']}", "",
             "현재 작업 폴더의 Ocarina VM을 Release 최적화로 컴파일하여 측정했습니다. 모든 언어의 반환 체크섬이 일치했습니다.", "",
             "## 실행 시간", "", "단위는 ms이며 작을수록 빠릅니다. 각 셀은 워밍업·배치 크기 보정 후 9개 표본의 중앙값입니다.", "",
             "| 작업 | C++ | C# | JavaScript | Python | Ocarina |", "| --- | ---: | ---: | ---: | ---: | ---: |"]
    for case, label in labels.items():
        lines.append("| " + label + " | " + " | ".join(f"{values[case, lang]['median_ms']:.3f}" for lang in languages) + " |")
    lines += ["", "## Ocarina의 상대 실행 시간", "", "예: 3.75배는 Ocarina가 해당 언어보다 같은 작업에 3.75배 오래 걸렸다는 뜻입니다.", "",
              "| 작업 | C++ 대비 | C# 대비 | JavaScript 대비 | Python 대비 |", "| --- | ---: | ---: | ---: | ---: |"]
    for case, label in labels.items():
        current = values[case, "Ocarina"]["median_ms"]
        lines.append("| " + label + " | " + " | ".join(f"{current / values[case, lang]['median_ms']:.2f}배" for lang in languages[:-1]) + " |")
    lines += ["", "## 환경과 방법", "",
              f"- CPU: {info['cpu']}; {info['os']}; 논리 프로세서 {info['logical_cpus']}개.",
              f"- 동일 논리 CPU에 실행을 고정했습니다. 프로세스 affinity mask: {info['affinity_mask']}.",
              f"- C++ 및 Ocarina: MSVC 도구 {info['msvc_tools']}, x64 Release `/O2 /GL /MD /fp:precise`, 측정 실행파일은 `/DEBUG:NONE`으로 링크했습니다.",
              f"- Python: {info['python']}; 실험적 JIT 활성화: {info['python_jit_enabled']}.",
              f"- JavaScript: Node.js {info['node']}, 기본 V8 JIT.",
              f"- C#: .NET SDK {info['dotnet']}, Release, tiered compilation 비활성화. 실행 중 최적화 단계가 바뀌는 변동을 줄였습니다.",
              "- 각 작업을 별도 프로세스에서 순차 실행했습니다. 언어·작업 순서는 고정 시드로 섞었습니다.",
              "- 최소 5회 워밍업 후 본 네 작업의 배치 시간을 약 50ms 이상으로 보정하고, 같은 배치 크기로 9회 측정했습니다. 빈 기준 작업에는 반복 횟수 상한이 적용됩니다.",
              "- 소스 컴파일, 바이트코드 로딩, 프로세스 시작, 콘솔 출력은 표의 실행 시간에서 제외했습니다.",
              "- Ocarina는 현재 VM 객체 파일에 전용 C++ 진입점을 연결했습니다. `OVM::run()`의 실행 상태 초기화·정리 비용은 포함합니다.",
              "- SDL 창 생성, GPU 그리기, 렌더 큐, 입력 이벤트는 측정하지 않았습니다.",
              "- 정수 중간값은 모든 언어에서 int32 범위 안에 머뭅니다. 실수 테스트는 모두 double/binary64입니다.",
              "- 객체 테스트는 객체 1개를 만들고 메서드로 상태를 20만 번 갱신합니다. 20만 개 객체 생성 테스트가 아닙니다.",
              "- 각 언어의 인라인·상수 나눗셈 최적화·객체 최적화를 허용합니다. C++은 최상위 커널에 noinline 및 volatile 입력을 사용해 배치 전체의 계산 제거를 방지합니다.",
              "", "## 체크섬과 측정 범위", "",
              "| 작업 | 공통 체크섬 | Ocarina 최소~최대 (ms) |", "| --- | ---: | ---: |"]
    for case, label in labels.items():
        row = values[case, "Ocarina"]
        lines.append(f"| {label} | {row['checksum']} | {row['min_ms']:.3f}~{row['max_ms']:.3f} |")
    baseline = values["baseline", "Ocarina"]["median_ms"] * 1000
    lines += ["", f"빈 `main()`에 가까운 Ocarina 기준 작업은 {baseline:.3f}µs였습니다. 프로세스 시작 시간이 아니라 VM 진입·정리의 기준 비용입니다.",
              "다른 언어의 빈 작업은 타이머·호출·최적화의 영향을 크게 받으므로 언어 간 순위 계산에서 제외했습니다.",
              "", "## 해석과 한계", "",
              "이 네 CPU 마이크로벤치마크에서는 Ocarina가 비교한 언어보다 느렸고, Python 대비 차이는 특히 재귀 호출에서 컸습니다.",
              "현재 VM은 바이트코드를 해석하고, 함수 호출마다 프레임과 지역 저장 공간을 관리합니다. 이 구조가 비용의 후보이지만 CPU 프로파일링으로 원인 비중을 측정한 결과는 아닙니다.",
              "이 결과를 게임 전체의 FPS, GPU 성능, 대량 객체 순회 또는 언어 전체의 보편적인 성능 순위로 일반화할 수 없습니다.",
              "C++/C#/JavaScript의 객체 메서드는 인라인되거나 객체 저장 자체가 최적화될 수 있습니다. 이는 최적화된 실제 실행 성능 비교이며, 강제로 동일한 호출 명령을 유지한 비교는 아닙니다.",
              "Windows의 스케줄링·전력 상태·백그라운드 작업 영향이 남아 있습니다. 일부 표본의 변동은 원시 JSON/CSV의 최소·최대·표준편차에서 확인할 수 있습니다.",
              "일반 Ocarina Release 실행파일의 초기 링크에서는 PDB DLL 버전 오류가 발생했습니다. 측정에는 정상 컴파일된 동일 Release VM 객체 파일과 디버그 심볼 없는 전용 하네스를 사용했습니다.",
              "", "## 재현", "", "프로젝트 루트에서 정상 동작하는 Python으로 실행합니다. Visual Studio C++ 빌드 도구, .NET 10 SDK, Node.js가 필요합니다. 외부 패키지는 다운로드하지 않습니다.",
              "인자 없이 실행하면 필요한 빌드, 전체 측정, 결과 검증, 보고서 저장까지 수행하고 마지막에 실행 시간·상대 성능 표를 터미널에 바로 출력합니다.",
              "", "```powershell", "python bench.py", "```", "",
              "이미 빌드한 실행파일로 다시 측정하려면 `python -B bench.py run`, 기존 결과 문서만 다시 만들려면 `python -B bench.py report`를 실행합니다.",
              "", "이 환경의 Python 실행파일:", "", f"`{info['python_executable']}`", "",
              "실행 진입점은 루트의 `bench.py`, 벤치마크 소스·하네스·결과는 `misc/benchmarks/`에 있습니다. 재생성 가능한 빌드 산출물은 무시되는 `misc/benchmarks/build/`에 생성됩니다. 제품 코드는 벤치마크를 위해 수정하지 않았습니다.",
              "원시 표본: [results.json](results/results.json), 집계 표: [results.csv](results/results.csv).", "",
              f"측정 소스 기준 Git HEAD: `{info['git_head']}`. 커밋되지 않은 제품 코드 변경을 포함합니다.",
              f"Ocarina C++/헤더/프로젝트 소스 SHA-256: `{info['source_sha256']}`.", ""]
    (HERE / "README.md").write_text("\n".join(lines), encoding="utf-8")
    print_results(values)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("action", nargs="?", default="all", choices=("build", "run", "all", "report"),
                        help="default: all (build, run benchmarks, and print results)")
    args = parser.parse_args()
    if args.action in ("build", "all"):
        build()
    if args.action in ("run", "all"):
        run()
    if args.action == "report":
        summarize()
