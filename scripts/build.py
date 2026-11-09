#!/usr/bin/env python3
"""Build in an isolated directory; never change the user's Conan configuration.

Use --toolchain to reuse an explicitly supplied, compatible Conan dependency cache.
Use --profile for a fresh dependency resolution with project-local Conan state.
"""

import argparse
import json
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def run(*args, cwd=ROOT, env=None):
    print("+", " ".join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=cwd, env=env, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--headless", action="store_true")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument(
        "--build-type",
        default="Release",
        choices=["Debug", "Release", "RelWithDebInfo"],
    )
    parser.add_argument("--adnn-root", type=Path, default=ROOT / ".deps")
    parser.add_argument(
        "--fetch-adnn",
        action="store_true",
        help="Clone pinned sources only into missing directories",
    )
    parser.add_argument("--toolchain", type=Path)
    parser.add_argument(
        "--profile",
        type=Path,
        help="Conan host profile (required unless --toolchain or --headless)",
    )
    parser.add_argument("--build-profile", type=Path)
    parser.add_argument("--conan", type=Path, default=ROOT / ".tools/conan1/bin/conan")
    parser.add_argument("--conan-home", type=Path, default=ROOT / ".tools/conan-home")
    parser.add_argument(
        "--lockfile", type=Path, help="Reuse an approved Conan 1 dependency lock"
    )
    parser.add_argument(
        "--build-all-deps",
        action="store_true",
        help="Rebuild dependencies on the target baseline",
    )
    parser.add_argument("--jobs", type=int, default=min(4, os.cpu_count() or 1))
    parser.add_argument("--skip-tests", action="store_true")
    args = parser.parse_args()
    build = (
        args.build_dir or ROOT / ("build-headless" if args.headless else "build-native")
    ).resolve()
    toolchain = args.toolchain.resolve() if args.toolchain else None
    if not args.headless:
        if args.fetch_adnn:
            args.adnn_root.mkdir(parents=True, exist_ok=True)
            sources = json.loads((ROOT / "cmake/adnn-sources.json").read_text())
            for name, source in sources.items():
                destination = args.adnn_root.resolve() / name
                if destination.exists():
                    print(f"Preserving existing checkout: {destination}")
                    continue
                run("git", "clone", "--no-checkout", source["url"], destination)
                run("git", "checkout", "--detach", source["revision"], cwd=destination)
                run(
                    "git",
                    "submodule",
                    "update",
                    "--init",
                    "--recursive",
                    cwd=destination,
                )
        if not toolchain:
            if not args.profile:
                parser.error(
                    "native builds require --profile or an explicit --toolchain; see docs/build.md"
                )
            environment = os.environ.copy()
            environment["CONAN_USER_HOME"] = str(args.conan_home.resolve())
            environment["CONAN_REVISIONS_ENABLED"] = "1"
            version = subprocess.check_output(
                [str(args.conan), "--version"], text=True, env=environment
            )
            if "1.66.0" not in version:
                parser.error("install Conan 1.66.0 in .tools/conan1; see docs/build.md")
            command = [
                args.conan.resolve(),
                "install",
                ROOT,
                "-if",
                build / "generators",
                "-pr:h",
                args.profile.resolve(),
                "-pr:b",
                (args.build_profile or args.profile).resolve(),
                "-s:h",
                f"build_type={args.build_type}",
                "--build",
                "*" if args.build_all_deps else "missing",
                "--lockfile-out",
                build / "conan.lock",
            ]
            if args.lockfile:
                command.extend(["--lockfile", args.lockfile.resolve()])
            run(*command, env=environment)
            toolchain = build / "generators/conan_toolchain.cmake"
    configure = [
        "cmake",
        "-S",
        ROOT,
        "-B",
        build,
        "-G",
        "Ninja",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=1",
        f"-DCMAKE_BUILD_TYPE={args.build_type}",
        f"-DCHIENPO_BUILD_NATIVE={'OFF' if args.headless else 'ON'}",
        f"-DCHIENPO_BUILD_TESTS={'OFF' if args.skip_tests else 'ON'}",
    ]
    if not args.headless:
        configure.extend(
            [
                f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
                f"-DCHIENPO_ADNN_ROOT={args.adnn_root.resolve()}",
            ]
        )
    run(*configure)
    run("cmake", "--build", build, "--parallel", args.jobs)
    if not args.skip_tests:
        run("ctest", "--test-dir", build, "--output-on-failure")


if __name__ == "__main__":
    main()
