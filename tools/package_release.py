#!/usr/bin/env python3
"""Assembles a distributable Horizon Unseen zip.

Deliberately a script rather than inline workflow YAML so the packaging can be
run and verified locally. Debugging a packaging bug through five-minute CI
round-trips is miserable, and this is the step most likely to go subtly wrong --
a missing shader or a stale atlas produces a build that launches and then
misbehaves.

    python tools/package_release.py --build build-release --version 0.3.0

Verifies as it goes and exits non-zero with a specific message on any problem.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]

# Everything a player needs, as (source relative to build dir, destination in
# the package). Missing entries are fatal: shipping without a shader produces a
# build that starts and then fails at draw time.
REQUIRED_ARTIFACTS = [
    ("HorizonUnseen.exe", "HorizonUnseen.exe"),
    # OpenAL Soft. Shipped as a DLL rather than statically linked, which is what
    # keeps its LGPL v2 licence cleanly separated from this MIT project.
    ("OpenAL32.dll", "OpenAL32.dll"),
    ("shaders/sprite.vert.spv", "shaders/sprite.vert.spv"),
    ("shaders/sprite.frag.spv", "shaders/sprite.frag.spv"),
    ("assets/atlas.png", "assets/atlas.png"),
    ("assets/atlas.json", "assets/atlas.json"),
    ("assets/sounds.json", "assets/sounds.json"),
]

# Directories copied wholesale, as (source relative to build dir, destination,
# glob, minimum count). Listing nineteen wav files individually would be a
# maintenance trap -- but a bare copy would silently ship an empty folder if the
# generator had never been run, so a floor is enforced instead.
REQUIRED_DIRECTORIES = [
    ("assets/sounds", "assets/sounds", "*.wav", 19),
]

# The RL library is a developer tool, not player-facing, so it is deliberately
# excluded from the distribution.
EXCLUDED_FROM_PACKAGE = ("husim.dll", "HorizonUnseenTests.exe")

README_TEMPLATE = REPO_ROOT / "packaging" / "README.txt"

# Non-system DLLs a correctly built release may depend on. vulkan-1.dll ships
# with GPU drivers and OpenAL32.dll is shipped alongside the executable;
# anything else means the static CRT did not take and the build will fail to
# start on a machine without Visual Studio.
ALLOWED_RUNTIME_DLLS = {"vulkan-1.dll", "openal32.dll"}


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def check_no_msvc_runtime_dependency(exe: Path) -> None:
    """Guards the mistake that would break the build for every player.

    A Debug build, or a Release build without the static CRT, links
    MSVCP140D.dll / VCRUNTIME140.dll. Those are absent on a machine without
    Visual Studio, so the game fails to launch with a cryptic system dialog --
    and it works perfectly on the developer's machine, so it is easy to ship.
    """
    dumpbin = shutil.which("dumpbin")
    if not dumpbin:
        print("  note: dumpbin not on PATH, skipping dependency check")
        return

    try:
        output = subprocess.run([dumpbin, "/dependents", str(exe)],
                                capture_output=True, text=True, timeout=120).stdout
    except Exception as exc:  # noqa: BLE001 - diagnostic only
        print(f"  note: dependency check could not run ({exc})")
        return

    suspicious = []
    for line in output.splitlines():
        name = line.strip()
        if not name.lower().endswith(".dll"):
            continue
        lowered = name.lower()
        if lowered.startswith(("msvcp", "vcruntime", "ucrtbase", "concrt")):
            suspicious.append(name)

    if suspicious:
        fail(
            "the executable depends on the MSVC runtime: "
            + ", ".join(suspicious)
            + "\nThose are not present on a machine without Visual Studio, so this "
              "build would fail to launch for players.\nConfigure with "
              "-DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded"
        )
    print("  no MSVC runtime dependency (static CRT confirmed)")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", required=True, help="build directory to package from")
    parser.add_argument("--version", required=True, help="version, e.g. 0.3.0 or v0.3.0")
    parser.add_argument("--out", default="dist", help="output directory")
    args = parser.parse_args()

    version = args.version.lstrip("v")
    build_dir = Path(args.build).resolve()
    out_dir = (REPO_ROOT / args.out).resolve()

    if not build_dir.is_dir():
        fail(f"build directory not found: {build_dir}")

    package_name = f"HorizonUnseen-v{version}-win64"
    stage = out_dir / package_name

    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)

    print(f"packaging {package_name}")
    print(f"  from {build_dir}")

    # --- Copy required artefacts -----------------------------------------
    missing = []
    for src_rel, dst_rel in REQUIRED_ARTIFACTS:
        src = build_dir / src_rel
        if not src.is_file():
            missing.append(src_rel)
            continue
        dst = stage / dst_rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)

    if missing:
        fail("missing required build artefacts:\n  " + "\n  ".join(missing))
    print(f"  copied {len(REQUIRED_ARTIFACTS)} artefacts")

    # --- Copy required directories ----------------------------------------
    copied_dir_files = []
    for src_rel, dst_rel, pattern, minimum in REQUIRED_DIRECTORIES:
        src_dir = build_dir / src_rel
        if not src_dir.is_dir():
            fail(f"missing required directory: {src_rel}\n"
                 f"Regenerate it with: python tools/generate_audio.py --out assets")

        found = sorted(src_dir.glob(pattern))
        if len(found) < minimum:
            fail(f"{src_rel} has {len(found)} {pattern} file(s), expected at least {minimum}.\n"
                 f"Regenerate them with: python tools/generate_audio.py --out assets")

        for src in found:
            dst = stage / dst_rel / src.name
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
            copied_dir_files.append(f"{dst_rel}/{src.name}")
        print(f"  copied {len(found)} file(s) from {src_rel}")

    check_no_msvc_runtime_dependency(stage / "HorizonUnseen.exe")

    # --- Player README ----------------------------------------------------
    if not README_TEMPLATE.is_file():
        fail(f"missing packaging template: {README_TEMPLATE}")

    readme = README_TEMPLATE.read_text(encoding="utf-8").replace("@VERSION@", version)
    (stage / "README.txt").write_text(readme, encoding="utf-8", newline="\r\n")
    if "@VERSION@" in readme:
        fail("README template still contains an unsubstituted placeholder")
    print("  wrote README.txt")

    # --- Zip --------------------------------------------------------------
    zip_path = out_dir / f"{package_name}.zip"
    if zip_path.exists():
        zip_path.unlink()

    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for path in sorted(stage.rglob("*")):
            if path.is_file():
                zf.write(path, f"{package_name}/{path.relative_to(stage)}")

    # --- Verify the zip ---------------------------------------------------
    with zipfile.ZipFile(zip_path) as zf:
        names = set(zf.namelist())
        if zf.testzip() is not None:
            fail("the produced zip is corrupt")

    expected = {f"{package_name}/{dst}" for _, dst in REQUIRED_ARTIFACTS}
    expected.update(f"{package_name}/{rel}" for rel in copied_dir_files)
    expected.add(f"{package_name}/README.txt")
    absent = sorted(expected - names)
    if absent:
        fail("zip is missing entries:\n  " + "\n  ".join(absent))

    leaked = sorted(n for n in names if Path(n).name in EXCLUDED_FROM_PACKAGE)
    if leaked:
        fail("developer-only files leaked into the package:\n  " + "\n  ".join(leaked))

    size_mb = zip_path.stat().st_size / (1024 * 1024)
    print(f"  verified {len(names)} entries")
    print(f"\n{zip_path}  ({size_mb:.2f} MB)")

    # Consumed by the workflow to attach the asset.
    print(f"::notice title=Package::{zip_path.name} ({size_mb:.2f} MB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
