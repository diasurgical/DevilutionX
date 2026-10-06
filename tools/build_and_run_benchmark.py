#!/usr/bin/env python

import subprocess
import argparse
import os
import sys
import pathlib
import shlex
import shutil
import platform

_PROFILE = "/tmp/out.profile"


def run(*args: list[str], env: dict[str, str] | None = None):
    print(
        "+",
        *(map(shlex.quote, [f"{k}={v}" for k, v in env.items()] if env else [])),
        *map(shlex.quote, args),
        file=sys.stderr,
    )
    full_env = None
    if env:
        full_env = os.environ.copy()
        for k, v in env.items():
            full_env[k] = v
    subprocess.run(args, stdout=sys.stdout, stderr=sys.stderr, check=True, env=full_env)


def nproc():
    return len(os.sched_getaffinity(0))


def maybe_create_build_dir(dir: str, args: list[str]):
    if os.path.isdir(dir):
        return
    print("Creating build directory at ", dir, file=sys.stderr)
    run("cmake", "-S.", f"-B{dir}", "-DCMAKE_BUILD_TYPE=RelWithDebInfo", *args)


def build_target(dir: str, target: str):
    run("cmake", "--build", dir, "-j", str(nproc()), "--target", target)


def run_benchmark(dir: str, target: str, benchmark_args: list[str], gperf: bool):
    args = []
    if platform.system() == "Linux":
        args.append("tools/linux_reduced_cpu_variance_run.sh")
    env = None
    if gperf:
        env: dict[str, str] = {"CPUPROFILE": _PROFILE}
        if not "CPUPROFILE_FREQUENCY" in env:
            env["CPUPROFILE_FREQUENCY"] = "1000"
    run(*args, f"{dir}/{target}", *benchmark_args, env=env)


def find_pprof() -> list[str]:
    if shutil.which("pprof"):
        return ["pprof"]
    if shutil.which("go"):
        return ["go", "tool", "pprof"]
    raise FileNotFoundError("pprof not found; install gperftools or Go")


def run_pprof(dir: str, target: str, pprof_args: list[str]):
    run(*find_pprof(), *pprof_args, f"{dir}/{target}", _PROFILE)


def main():
    os.chdir(pathlib.Path(__file__).resolve().parent.parent)
    parser = argparse.ArgumentParser(description="Builds and runs a benchmark")
    parser.add_argument("-B", "--build", help="build directory")
    parser.add_argument(
        "--gperf", action=argparse.BooleanOptionalAction, help="profile with gperftools"
    )
    parser.add_argument("--port", type=int, default=1337, help="pprof server port (default http mode)")
    parser.add_argument(
        "--pprof-args",
        nargs="+",
        metavar="ARG",
        help="arguments passed to pprof (default: --http=localhost:PORT)",
    )
    parser.add_argument("target", help="benchmark target")
    parser.add_argument(
        "benchmark_args",
        nargs="*",
        help="arguments passed to the benchmark binary",
    )
    parser.add_argument("--run", action=argparse.BooleanOptionalAction, default=True, help="If false, only builds the target")
    args = parser.parse_args()
    build = args.build
    if not build:
        build = "build-gperf" if args.gperf else "build-reld"
    configure_args = []
    if args.gperf:
        configure_args.append("-DGPERF=ON")
    pprof_args = args.pprof_args if args.pprof_args else [f"--http=localhost:{args.port}"]
    try:
        maybe_create_build_dir(build, configure_args)
        build_target(build, args.target)
        if args.run:
            run_benchmark(build, args.target, args.benchmark_args, args.gperf)
            if args.gperf:
                run_pprof(build, args.target, pprof_args)
    except subprocess.CalledProcessError as e:
        print("Error:", e.cmd[0], "failed", file=sys.stderr)
        return e.returncode
    except KeyboardInterrupt as e:
        return 1


main()
