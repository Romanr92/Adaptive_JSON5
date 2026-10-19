#!/usr/bin/env python3
"""Build and bundle the installed static parser, without host test dependencies."""

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile


PROFILES = {
    "linux-x86_64-gcc": ("Linux", r"x86_64-.*linux.*"),
    "linux-aarch64-gcc": ("Linux", r"aarch64-.*linux.*"),
    "windows-x86_64-mingw-gcc": ("Windows", r"x86_64-.*mingw32"),
    "adaptivepi-aarch64-yocto5.0.20-sdk0.2.0-gcc": ("Linux", r"aarch64-poky-linux"),
}
SDK_SHA256 = "abd5aeb0b6243362c8fafc88c33eb501f62354add112a6509d0ec974cec017d6"


def run(*args, cwd=None):
    """Run arguments directly so version/profile strings never enter a shell."""
    return subprocess.check_output([str(arg) for arg in args], cwd=cwd, text=True).strip()


def release_version(tag):
    """Return the CMake numeric version, retaining the caller's exact tag elsewhere."""
    match = re.fullmatch(r"v?(\d+\.\d+\.\d+)(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?", tag)
    if not match:
        raise ValueError("Use a MAJOR.MINOR.PATCH release tag, optionally prefixed with v")
    return match[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=PROFILES, required=True)
    parser.add_argument("--version", required=True, help="Exact release tag, e.g. 0.1.0, v0.1.0 or 0.1.0-rc.1")
    parser.add_argument("--source-dir", type=Path, help="Released source checkout; defaults to this tool's repository")
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=Path("dist"))
    parser.add_argument("--compiler", help="GNU C++ compiler; omit when using a sourced SDK")
    parser.add_argument("--toolchain", type=Path)
    parser.add_argument("--expected-commit", help="Release event commit to build")
    parser.add_argument("--sdk-sha256", default="")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--allow-dirty", action="store_true", help="Local development only; never publish this output")
    args = parser.parse_args()
    tooling = Path(__file__).resolve().parents[1]
    source = args.source_dir.resolve() if args.source_dir else tooling
    try:
        version = release_version(args.version)
    except ValueError as error:
        parser.error(str(error))
    project = re.search(r"project\(AdaptiveJSON5 VERSION ([0-9.]+)", (source / "CMakeLists.txt").read_text())
    if not project or project[1] != version:
        parser.error("Release tag must match project(AdaptiveJSON5 VERSION ...) in CMakeLists.txt")
    revision = run("git", "rev-parse", "HEAD", cwd=source)
    if args.expected_commit and revision != args.expected_commit:
        parser.error("Checked-out source differs from the release event commit")
    dirty = bool(run("git", "status", "--porcelain", "--untracked-files=normal", cwd=source))
    tooling_dirty = bool(run("git", "status", "--porcelain", "--untracked-files=normal", cwd=tooling))
    if (dirty or tooling_dirty) and not args.allow_dirty:
        parser.error("Release source and packaging tools must be clean; use --allow-dirty only for unpublished local validation")
    sdk_profile = args.platform.startswith("adaptivepi-")
    if sdk_profile and (args.sdk_sha256 != SDK_SHA256 or not args.toolchain):
        parser.error("AdaptivePi profile requires the pinned v0.2.0 SDK checksum and its CMake toolchain file")
    if not sdk_profile and args.sdk_sha256:
        parser.error("An SDK checksum belongs only to the AdaptivePi SDK profile")
    build = args.build_dir.resolve()
    if build == source or build in source.parents:
        parser.error("Build directory must not be the source directory or an ancestor")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    configure = [
        "cmake", "--fresh", "-S", source, "-B", build, "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
        "-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF",
        "-DADAPTIVE_JSON5_BUILD_TESTS=OFF", "-DADAPTIVE_JSON5_BUILD_LIBRARY=ON",
        "-DADAPTIVE_JSON5_BUILD_SCHEMA=OFF", "-DADAPTIVE_JSON5_BUILD_EXECUTABLE=OFF",
        "-DADAPTIVE_JSON5_ENABLE_EXCEPTIONS=OFF", "-DADAPTIVE_JSON5_ENABLE_RTTI=OFF",
        "-DCMAKE_INSTALL_LIBDIR=lib", "-DCMAKE_INSTALL_INCLUDEDIR=include",
        "-DADAPTIVE_JSON5_INSTALL_ARCHIVEDIR=.",
        f"-DADAPTIVE_JSON5_PACKAGE_PROFILE={args.platform}",
        f"-DADAPTIVE_JSON5_SDK_SHA256={args.sdk_sha256}",
    ]
    if args.compiler:
        configure.append(f"-DCMAKE_CXX_COMPILER={args.compiler}")
    if args.toolchain:
        configure.append(f"-DCMAKE_TOOLCHAIN_FILE={args.toolchain.resolve()}")
    subprocess.run([str(arg) for arg in configure], check=True)
    info = json.loads((build / "build-info.json").read_text())
    system, triple = PROFILES[args.platform]
    if info["compiler_id"] != "GNU" or info["system"] != system or not re.fullmatch(triple, info["target_triple"]):
        parser.error(f"Selected compiler does not build {args.platform}: {info['compiler_id']} {info['system']} {info['target_triple']}")
    if info["glibcxx_cxx11_abi"] not in ("0", "1"):
        parser.error("Unable to identify libstdc++ string ABI")
    if info["source_revision"] != revision:
        parser.error("CMake source revision differs from the packaged source")
    subprocess.run(["cmake", "--build", str(build), "--parallel", str(args.jobs)], check=True)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    name = f"AdaptiveJSON5-{args.version}-{args.platform}"
    archive = args.output_dir.resolve() / f"{name}.tar.gz"
    with tempfile.TemporaryDirectory(prefix="adaptive-json5-package-", dir=build.parent) as directory:
        stage = Path(directory) / name
        subprocess.run(["cmake", "--install", str(build), "--prefix", str(stage)], check=True)
        if not (stage / "libadaptive_json5_parser.a").is_file():
            parser.error("Installed static .a archive is missing")
        expected = {p.relative_to(source / "includes") for p in (source / "includes").rglob("*.hpp")}
        actual = {p.relative_to(stage / "include") for p in (stage / "include").rglob("*.hpp")}
        if expected != actual:
            parser.error("Installed public-header inventory differs from supported headers")
        info.update({"release_tag": args.version, "source_dirty": dirty,
                     "packaging_revision": run("git", "rev-parse", "HEAD", cwd=tooling),
                     "packaging_dirty": tooling_dirty,
                     "position_independent_code": True, "lto": False,
                     "cmake_version": run("cmake", "--version").splitlines()[0]})
        # The compiler/system versions are in build-info; host tools are not runtime dependencies.
        (stage / "build-info.json").write_text(json.dumps(info, indent=2) + "\n", encoding="utf-8")
        # Recovery builds keep library sources immutable while shipping the corrected
        # consumer import helper. Its separate tooling revision is recorded above.
        shutil.copyfile(tooling / "cmake/ImportAdaptiveJSON5.cmake",
                        stage / "share/AdaptiveJSON5/cmake/ImportAdaptiveJSON5.cmake")
        timestamp = int(run("git", "show", "-s", "--format=%ct", "HEAD", cwd=source))
        # Normalize archive metadata, not library bytes: toolchain reproducibility is separate.
        with archive.open("wb") as output, gzip.GzipFile(fileobj=output, mode="wb", filename="", mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w") as bundle:
                for path in sorted(stage.rglob("*")):
                    entry = bundle.gettarinfo(str(path), f"{name}/{path.relative_to(stage).as_posix()}")
                    entry.uid = entry.gid = 0
                    entry.uname = entry.gname = ""
                    entry.mtime = timestamp
                    entry.mode = 0o755 if path.is_dir() else 0o644
                    if path.is_file():
                        with path.open("rb") as content:
                            bundle.addfile(entry, content)
                    else:
                        bundle.addfile(entry)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix(archive.suffix + ".sha256").write_text(f"{digest}  {archive.name}\n", encoding="utf-8")
    archive.with_suffix(archive.suffix + ".json").write_text(json.dumps({
        "schema_version": 1, "asset": archive.name, "sha256": digest, "build": info,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Package: {archive}\nSHA-256: {digest}")


if __name__ == "__main__":
    main()
