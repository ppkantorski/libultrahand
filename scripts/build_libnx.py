#!/usr/bin/env python3
"""
build_libnx.py

Builds and installs libnx for the Nintendo Switch (devkitA64 / devkitPro)
from the official switchbrew/libnx source, with two local patches applied
on top of stock upstream:

  nx/Makefile
    - Removes -Werror from CFLAGS (keeps -Wall), so upstream compiler
      warnings don't hard-fail the build.
    - Replaces the release BUILD_CFLAGS from "-DNDEBUG=1 -O2" with a
      size-focused flag set: -Os -fomit-frame-pointer -fdata-sections
      -ffunction-sections -ffast-math -finline-small-functions
      -fno-strict-aliasing -frename-registers -falign-functions=16

  nx/switch.ld
    - Narrows the .eh_frame KEEP rule so only crtbegin.o's and
      crtend.o's .eh_frame entries (the unwind-table terminators) are
      kept; every other object's .eh_frame content is discarded. This
      strips per-object C++ exception-unwind tables out of the final
      binary.

Both patches are embedded below as unified diffs and applied with
`patch -p1` against a fresh checkout of upstream libnx.

Usage:
    python3 build_libnx.py [options]

Run from an empty working directory.

Requirements:
  - devkitPro pacman with devkitA64 installed
  - DEVKITPRO environment variable set (source your devkitPro env)
  - host tools: make, patch, tar

Steps performed:
  1. Downloads the libnx source tree from GitHub for the chosen ref
     (--ref, default "master"; accepts a branch, tag, or commit SHA)
  2. Applies the two patches described above
  3. make clean && make -jN
  4. Backs up the currently installed $DEVKITPRO/libnx, then installs
     the new build (sudo -E make install)
  5. Verifies the install and prints an old-vs-new libnx.a size diff

Note on PREFIX: libnx's Makefile does not actually read a PREFIX
variable -- its install target always writes to
$(DESTDIR)$(DEVKITPRO)/libnx. This script still sets PREFIX when
installing for compatibility with the conventional invocation, but
DEVKITPRO is what actually controls the install location.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tarfile
import time
import urllib.error
import urllib.request
from pathlib import Path

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

REPO = "switchbrew/libnx"
DEFAULT_REF = "master"

# Fixed local name for the extracted source tree regardless of --ref, so
# --keep has something stable to look for.
SRC_DIRNAME = "libnx-src"

# The two local patches applied on top of stock upstream libnx. Embedded
# as unified diffs and applied with `patch -p1` against a fresh checkout.

MAKEFILE_PATCH = """--- a/nx/Makefile
+++ b/nx/Makefile
@@ -26,7 +26,7 @@
 #---------------------------------------------------------------------------------
 ARCH   :=  -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIC -ftls-model=local-exec
 
-CFLAGS :=  -g -Wall -Werror \\
+CFLAGS :=  -g -Wall \\
            -ffunction-sections \\
            -fdata-sections \\
            $(ARCH) \\
@@ -112,7 +112,7 @@
 
 lib/libnx.a : lib release $(SOURCES) $(INCLUDES)
    @$(MAKE) BUILD=release OUTPUT=$(CURDIR)/$@ \\
-   BUILD_CFLAGS="-DNDEBUG=1 -O2" \\
+   BUILD_CFLAGS="-DNDEBUG=1 -Os -fomit-frame-pointer -fdata-sections -ffunction-sections -ffast-math -finline-small-functions -fno-strict-aliasing -frename-registers -falign-functions=16" \\
    DEPSDIR=$(CURDIR)/release \\
    --no-print-directory -C release \\
    -f $(CURDIR)/Makefile
@@ -153,4 +153,3 @@
 #---------------------------------------------------------------------------------------
 endif
 #---------------------------------------------------------------------------------------
-
"""

SWITCHLD_PATCH = """--- a/nx/switch.ld
+++ b/nx/switch.ld
@@ -77,7 +77,7 @@
 
    .gcc_except_table  : { *(.gcc_except_table .gcc_except_table.*) } :rodata
    .eh_frame_hdr      : { *(.eh_frame_hdr) *(.eh_frame_entry .eh_frame_entry.*) } :rodata
-   .eh_frame          : { KEEP (*(.eh_frame)) *(.eh_frame.*) } :rodata
+   .eh_frame : { KEEP (*crtbegin.o(.eh_frame)) KEEP (*crtend.o(.eh_frame)) } :rodata
    .gnu_extab         : { *(.gnu_extab*) } : rodata
    .exception_ranges  : { *(.exception_ranges .exception_ranges*) } :rodata
 
@@ -192,7 +192,7 @@
       ================== */
 
    /* Discard sections that difficult post-processing */
-   /DISCARD/ : { *(.group .comment .note) }
+   /DISCARD/ : { *(.group .comment .note) *(.eh_frame) *(.eh_frame.*) }
 
    /* Stabs debugging sections. */
    .stab          0 : { *(.stab) }
"""

PATCHES = {
    "nx/Makefile": MAKEFILE_PATCH,
    "nx/switch.ld": SWITCHLD_PATCH,
}


def log(msg):
    print(f"\n\033[1;36m==> {msg}\033[0m")


def run(cmd, cwd=None, env=None, check=True, capture=False, input=None):
    printable = cmd if isinstance(cmd, str) else " ".join(cmd)
    print(f"$ {printable}")
    result = subprocess.run(
        cmd,
        cwd=cwd,
        env=env,
        shell=isinstance(cmd, str),
        text=True,
        capture_output=capture,
        input=input,
    )
    if check and result.returncode != 0:
        if capture:
            sys.stderr.write(result.stdout or "")
            sys.stderr.write(result.stderr or "")
        sys.exit(f"\n[FAILED] exit code {result.returncode}: {printable}")
    return result


def check_host_tools():
    missing = [t for t in ("make", "patch", "tar") if shutil.which(t) is None]
    if missing:
        sys.exit(
            "Missing host build tools: " + ", ".join(missing) +
            "\nInstall them with your system package manager first "
            "(e.g. apt install make patch tar)."
        )


def resolve_ref(ref: str) -> str:
    """
    Best-effort: resolve a branch/tag name to its current commit SHA via
    the GitHub API, purely so the printed log shows exactly what was
    built. If this fails for any reason (rate limit, offline, etc.) it
    falls back to building the ref name as given -- not fatal.
    """
    try:
        req = urllib.request.Request(
            f"https://api.github.com/repos/{REPO}/commits/{ref}",
            headers={"Accept": "application/vnd.github+json",
                     "User-Agent": "build_libnx.py"},
        )
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read().decode())
            return data["sha"]
    except Exception as e:
        print(f"  (couldn't resolve '{ref}' to a commit SHA via the GitHub API: {e} -- continuing anyway)")
        return ref


def download_source(ref: str, workdir: Path) -> Path:
    """
    Downloads the libnx source tree for `ref` (branch, tag, or commit
    SHA) as a tarball and extracts it to workdir/SRC_DIRNAME.
    """
    url = f"https://github.com/{REPO}/archive/{ref}.tar.gz"
    tarball = workdir / f"libnx-{ref.replace('/', '_')}.tar.gz"

    log(f"Downloading libnx source ({REPO}@{ref})")
    print(f"  {url}")
    try:
        urllib.request.urlretrieve(url, tarball)
    except urllib.error.HTTPError as e:
        sys.exit(f"Download failed ({e.code}) -- is '{ref}' a real branch, tag, or commit SHA in {REPO}?")

    extract_root = workdir / "_extract"
    if extract_root.exists():
        shutil.rmtree(extract_root)
    extract_root.mkdir()

    with tarfile.open(tarball) as tf:
        tf.extractall(extract_root)

    entries = [p for p in extract_root.iterdir() if p.is_dir()]
    if len(entries) != 1:
        sys.exit(f"Expected exactly one top-level directory in the tarball, found: {entries}")
    extracted = entries[0]

    srcdir = workdir / SRC_DIRNAME
    if srcdir.exists():
        shutil.rmtree(srcdir)
    shutil.move(str(extracted), str(srcdir))
    shutil.rmtree(extract_root, ignore_errors=True)
    tarball.unlink(missing_ok=True)

    return srcdir


def apply_patches(srcdir: Path, workdir: Path):
    for relpath, patch_text in PATCHES.items():
        patchfile = workdir / (Path(relpath).name + ".patch")
        patchfile.write_text(patch_text)
        log(f"Applying patch to {relpath}")
        run(["patch", "-p1", "-i", str(patchfile)], cwd=srcdir)


def build_devkitpro_env() -> dict:
    devkitpro = os.environ.get("DEVKITPRO")
    if not devkitpro:
        sys.exit("DEVKITPRO is not set. Source your devkitPro environment first, "
                  "the same way you would before building libnx normally.")
    devkitpro = str(Path(devkitpro))

    devkita64_gcc = Path(devkitpro) / "devkitA64" / "bin" / "aarch64-none-elf-gcc"
    if not devkita64_gcc.exists():
        sys.exit(f"Can't find {devkita64_gcc} -- is devkitA64 installed via (dkp-)pacman?")

    env = os.environ.copy()
    env.update({
        "DEVKITPRO": devkitpro,
        "PATH": f"{devkitpro}/devkitA64/bin:{env.get('PATH', '')}",
    })
    return env


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 4,
                     help="parallel make jobs (default: nproc)")
    ap.add_argument("--ref", default=DEFAULT_REF,
                     help=f"libnx branch, tag, or commit SHA to build (default: {DEFAULT_REF})")
    ap.add_argument("--no-install", action="store_true",
                     help="build only, skip the sudo install step")
    ap.add_argument("--keep", action="store_true",
                     help="reuse an existing extracted+patched source dir instead of re-downloading")
    args = ap.parse_args()

    check_host_tools()
    env = build_devkitpro_env()
    workdir = Path.cwd()
    srcdir = workdir / SRC_DIRNAME

    if args.keep and srcdir.exists():
        log(f"--keep passed, reusing existing {srcdir.name}")
    else:
        resolved = resolve_ref(args.ref)
        if resolved != args.ref:
            print(f"  '{args.ref}' -> {resolved}")
        srcdir = download_source(resolved, workdir)
        apply_patches(srcdir, workdir)

    log(f"make clean; make -j{args.jobs}")
    run(["make", "clean"], cwd=srcdir, env=env)
    run(["make", f"-j{args.jobs}"], cwd=srcdir, env=env)

    new_lib = srcdir / "nx" / "lib" / "libnx.a"
    if not new_lib.exists():
        sys.exit(f"Build finished but couldn't find {new_lib} -- check the make output above.")

    log(f"Built {new_lib} ({new_lib.stat().st_size:,} bytes)")

    if args.no_install:
        log("--no-install passed, stopping here. Built library is at:")
        print(f"  {new_lib}")
        return

    # --- Back up whatever's currently installed, then install the new build ---
    installed_dir = Path(env["DEVKITPRO"]) / "libnx"
    old_lib = installed_dir / "lib" / "libnx.a"
    old_size = old_lib.stat().st_size if old_lib.exists() else None

    if installed_dir.exists():
        backup_dir = workdir / f"libnx-backup-{time.strftime('%Y%m%d-%H%M%S')}"
        log(f"Backing up currently installed {installed_dir} to {backup_dir}")
        shutil.copytree(installed_dir, backup_dir)
        print("To roll back later: dkp-pacman -S libnx (forces a reinstall from the official package)")

    log("Installing (requires sudo)")
    run(["sudo", "-E", "make", f"PREFIX={installed_dir}", "install"], cwd=srcdir, env=env)

    # --- Verify ---
    log("Verifying the install")
    installed_switchld = installed_dir / "switch.ld"
    if installed_switchld.exists() and "crtbegin.o" in installed_switchld.read_text():
        print("  [ok] installed switch.ld has the patched .eh_frame rule (crtbegin.o/crtend.o only)")
    else:
        print("  [!] installed switch.ld does NOT show the expected patch -- something's off")

    new_installed_lib = installed_dir / "lib" / "libnx.a"
    new_size = new_installed_lib.stat().st_size if new_installed_lib.exists() else None
    if new_size is not None:
        print(f"\n  libnx.a size: {new_size:,} bytes"
              + (f"  (was {old_size:,} bytes, {new_size - old_size:+,} bytes)" if old_size else "  (no previous install found)"))

    log("Done. Rebuild your homebrew projects normally.")


if __name__ == "__main__":
    main()