#!/usr/bin/env python3
"""Dependency-free development entry point. stdout is one JSON report."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def doctor():
    checks = []
    for label, command in (("make", "make"), ("compiler", os.environ.get("CC", "cc"))):
        words = shlex.split(command)
        found = bool(words and shutil.which(words[0]))
        checks.append({"name": label, "status": "passed" if found else "blocked",
                       "detail": command})
    overridden = bool(os.environ.get("RAYLIB_LIBS"))
    if overridden:
        checks.append({"name": "raylib", "status": "passed",
                       "detail": "RAYLIB_LIBS supplied; compilation will validate it"})
    else:
        found = shutil.which("pkg-config") is not None
        found = found and subprocess.run(
            ["pkg-config", "--exists", "raylib"], cwd=ROOT, timeout=30).returncode == 0
        checks.append({"name": "raylib", "status": "passed" if found else "blocked",
                       "detail": "pkg-config raylib (or set RAYLIB_CFLAGS and RAYLIB_LIBS)"})
    return checks


def run_step(name, command, out, timeout):
    log = out / (name + ".log")
    started = time.monotonic()
    print("Running " + name, file=sys.stderr, flush=True)
    with log.open("w") as stream:
        try:
            with subprocess.Popen(command, cwd=ROOT, stdout=stream,
                                  stderr=subprocess.STDOUT,
                                  start_new_session=True) as process:
                try:
                    code = process.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    # Stop the compiler/test children too, not just Make.
                    if os.name == "posix":
                        os.killpg(process.pid, signal.SIGKILL)
                    else:
                        process.kill()
                    process.wait()
                    raise
            status = "passed" if code == 0 else "failed"
        except FileNotFoundError as error:
            stream.write(str(error) + "\n")
            status, code = "blocked", None
        except subprocess.TimeoutExpired:
            stream.write("\nStep exceeded timeout.\n")
            status, code = "failed", None
    return {"name": name, "status": status, "exit_code": code,
            "seconds": round(time.monotonic() - started, 3), "log": str(log)}


def compilation_database(output):
    entries = []
    for line in output.splitlines():
        args = shlex.split(line)
        if "-c" not in args or "-o" not in args:
            continue
        sources = [arg for arg in args if arg.endswith(".c")]
        if len(sources) != 1:
            continue
        entries.append({"directory": str(ROOT), "file": str(ROOT / sources[0]),
                        "arguments": args})
    if not entries:
        raise ValueError("make produced no C compilation commands")
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("doctor", "check", "compdb"))
    parser.add_argument("--sanitizers", action="store_true", help="also run ASan/UBSan tests")
    parser.add_argument("--timeout", type=int, default=600, help="seconds allowed per check")
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    report = {"version": 1, "command": args.command, "checks": []}
    if args.command in ("doctor", "check"):
        report["checks"] = doctor()
    if any(item["status"] == "blocked" for item in report["checks"]):
        report["status"] = "blocked"
    elif args.command == "check":
        out = ROOT / "build" / "checks"
        out.mkdir(parents=True, exist_ok=True)
        steps = [("tooling", [sys.executable, "-m", "unittest", "discover", "-s", "tools", "-p", "test_*.py"]),
                 ("release", ["make", "-B", "release", "EXTRA_CFLAGS=-Werror"]),
                 ("tests", ["make", "test", "EXTRA_CFLAGS=-Werror"]),
                 ("release-tests", ["make", "test-release", "EXTRA_CFLAGS=-Werror"])]
        if args.sanitizers:
            steps.append(("sanitizers", ["make", "test-asan", "EXTRA_CFLAGS=-Werror"]))
        for name, command in steps:
            item = run_step(name, command, out, args.timeout)
            report["checks"].append(item)
            if item["status"] != "passed":
                break
        report["status"] = report["checks"][-1]["status"]
    elif args.command == "compdb":
        result = subprocess.run(["make", "-Bn", "debug"], cwd=ROOT, text=True,
                                capture_output=True, timeout=args.timeout, check=True)
        entries = compilation_database(result.stdout)
        path = ROOT / "compile_commands.json"
        path.write_text(json.dumps(entries, indent=2) + "\n")
        report.update(status="passed", path=str(path), entries=len(entries))
    else:
        report["status"] = "passed"
    if args.command == "check":
        out = ROOT / "build" / "checks"
        out.mkdir(parents=True, exist_ok=True)
        (out / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return {"passed": 0, "failed": 1, "blocked": 2}[report["status"]]


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(json.dumps({"version": 1, "status": "failed", "error": str(error)}))
        sys.exit(1)
