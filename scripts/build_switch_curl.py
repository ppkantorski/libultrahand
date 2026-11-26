#!/usr/bin/env python3
"""
build_switch_curl.py

Rebuilds libcurl for the Switch (devkitA64 / devkitPro) from the exact
source devkitPro's own `switch-curl` package uses -- curl 7.69.1 plus
their libnx TLS-backend patch (lib/vtls/libnx.c, the thing that lets curl
do HTTPS through the console's ssl: sysmodule instead of bundling
mbedtls) -- but with protocol handlers most Switch homebrew apps never
use (FTP, TELNET, DICT, GOPHER, TFTP, RTSP, IMAP, POP3, SMTP, SMB, LDAP,
MQTT, FILE) compiled out at ./configure time instead of just left
unused-but-linked. If your app does use one of these, remove it from
DISABLE_PROTOCOLS near the top of this file.

Why this can't be a Makefile flag in your app: curl keeps a single
static protocol dispatch table (Curl_builtin in lib/url.c) that
references every compiled-in protocol's handler struct, so
--gc-sections in YOUR Makefile can never drop them -- they're always
reachable from that one table. The only place to remove them is here,
at curl's own configure step.

--- Cross-compile bug fix (curl/curl#5126) ---
curl 7.69.1's m4/curl-functions.m4 has a broken wrapper macro:

    AC_DEFUN([CURL_RUN_IFELSE], [
       AC_REQUIRE([AC_RUN_IFELSE])dnl
       ...

AC_REQUIRE([AC_RUN_IFELSE]) is invalid usage -- it invokes AC_RUN_IFELSE
with no test program and no cross-compiling fallback. Under autoconf
2.69 this was a silent no-op; under autoconf >=2.70 it unconditionally
fails with "cannot run test program while cross compiling" before any
real check runs. This is a genuine curl bug (never backported to the
7.69.1 branch), fixed upstream by dropping that one line
(https://github.com/curl/curl/pull/5130). devkitPro's own
switch-curl.patch does not touch this file, so it's still broken there
too -- their build server just happens to use an old pinned autoconf.
This script applies the same one-line upstream fix locally, so you can
use whatever autoconf you already have (2.70+ is fine) -- no need to
hunt down autoconf 2.69.

Run this from an empty scratch directory:

    python3 build_switch_curl.py

Prerequisites (all standard for anyone already building devkitPro
portlibs):
  - devkitPro pacman with devkitA64, libnx, and switch-zlib installed
  - DEVKITPRO environment variable set (source your normal devkitPro env)
  - host build tools: autoconf, automake, libtool, pkg-config, patch, tar

What it does, in order:
  1. Downloads curl-7.69.1.tar.xz + devkitPro's switch-curl.patch,
     verifies both against the sha256 pinned in devkitPro's own PKGBUILD
  2. Extracts, applies devkitPro's patch
  3. Applies the one-line upstream fix for curl/curl#5126 (cross-compile
     AC_REQUIRE bug) to m4/curl-functions.m4
  4. Sets up the same cross-compile environment switchvars.sh would
  5. ./buildconf && ./configure (devkitPro's exact flags + the extra
     --disable-<protocol> flags configured below) && make -C lib
  6. Backs up your current installed libcurl.a/headers, then installs
     the new static lib into $DEVKITPRO/portlibs/switch (needs sudo,
     same as your libnx install flow)
  7. Verifies with aarch64-none-elf-nm that the disabled protocols are
     actually gone, and prints an old-vs-new .a size comparison

After this finishes, just rebuild your homebrew project as normal --
nothing in your app's Makefile needs to change, it links -lcurl
exactly like before.
"""

import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

# ---------------------------------------------------------------------------
# Config -- edit DISABLE_PROTOCOLS here if you ever need one of these back
# ---------------------------------------------------------------------------

CURL_VERSION = "7.69.1"
CURL_URL = f"https://curl.se/download/curl-{CURL_VERSION}.tar.xz"
CURL_SHA256 = "03c7d5e6697f7b7e40ada1b2256e565a555657398e6c1fcfa4cb251ccd819d4f"

PATCH_URL = ("https://raw.githubusercontent.com/devkitPro/pacman-packages/"
             "master/switch/curl/switch-curl.patch")
PATCH_SHA256 = "723c7d884fc7c39ae1a3115ba245bb8c1415da47bbd60ab8f943ca98f92ebc9a"

# Defaults assume your app only ever does CURLOPT_URL http(s):// calls,
# which covers most Switch homebrew (downloading files, hitting an API,
# etc). Before relying on this, grep your own source for curl_easy_setopt
# calls to confirm you don't use any of the protocols below -- if you do,
# just remove them from this list.
DISABLE_PROTOCOLS = [
    "ftp", "file", "ldap", "ldaps", "rtsp", "dict",
    "telnet", "tftp", "pop3", "imap", "smtp", "smb", "gopher", "mqtt",
]

# A handful of symbols we expect to be GONE from the rebuilt libcurl.a if
# the disables actually took effect. Used for the post-build sanity check.
CANARY_SYMBOLS = [
    "ftp_do", "telnet_do", "dict_do", "gopher_do", "tftp_do",
    "pop3_do", "imap_do", "smtp_do", "file_do",
]

# The broken line from curl/curl#5126 -- see module docstring above.
BROKEN_AC_REQUIRE_LINE = "AC_REQUIRE([AC_RUN_IFELSE])dnl"


def log(msg):
    print(f"\n\033[1;36m==> {msg}\033[0m")


def run(cmd, cwd=None, env=None, check=True, capture=False):
    printable = cmd if isinstance(cmd, str) else " ".join(cmd)
    print(f"$ {printable}")
    result = subprocess.run(
        cmd,
        cwd=cwd,
        env=env,
        shell=isinstance(cmd, str),
        text=True,
        capture_output=capture,
    )
    if check and result.returncode != 0:
        if capture:
            sys.stderr.write(result.stdout or "")
            sys.stderr.write(result.stderr or "")
        sys.exit(f"\n[FAILED] exit code {result.returncode}: {printable}")
    return result


def sha256_of(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def download(url: str, dest: Path, expected_sha256: str):
    if dest.exists() and sha256_of(dest) == expected_sha256:
        log(f"{dest.name} already present and verified, skipping download")
        return
    log(f"Downloading {url}")
    tmp = dest.with_suffix(dest.suffix + ".part")
    urllib.request.urlretrieve(url, tmp)
    actual = sha256_of(tmp)
    if actual != expected_sha256:
        tmp.unlink(missing_ok=True)
        sys.exit(
            f"Checksum mismatch for {dest.name}\n"
            f"  expected: {expected_sha256}\n"
            f"  got:      {actual}\n"
            f"The upstream file or devkitPro's patch may have changed -- "
            f"stopping rather than building against something unverified."
        )
    tmp.rename(dest)
    log(f"Checksum OK for {dest.name}")


def check_host_tools():
    missing = [t for t in ("autoconf", "automake", "libtool", "pkg-config", "patch", "tar", "make")
               if shutil.which(t) is None]
    if missing:
        sys.exit(
            "Missing host build tools: " + ", ".join(missing) +
            "\nInstall them with your system package manager first "
            "(e.g. apt install autoconf automake libtool pkg-config patch build-essential, "
            "or on macOS: brew install autoconf automake libtool pkg-config)."
        )


def fix_cross_compile_bug(srcdir: Path):
    """
    Fixes curl/curl#5126: m4/curl-functions.m4's CURL_RUN_IFELSE wrapper
    does AC_REQUIRE([AC_RUN_IFELSE]), which is invalid and makes any
    cross-compiling ./configure hard-fail with "cannot run test program
    while cross compiling" under autoconf >=2.70. Fixed upstream in
    https://github.com/curl/curl/pull/5130 by deleting that one line.
    """
    target = srcdir / "m4" / "curl-functions.m4"
    if not target.exists():
        sys.exit(f"Expected {target} to exist -- curl's source layout may have "
                  f"changed, can't apply the cross-compile fix blindly.")

    text = target.read_text()
    lines = text.splitlines(keepends=True)
    matched = [i for i, line in enumerate(lines) if BROKEN_AC_REQUIRE_LINE in line]

    if not matched:
        # Already patched (e.g. re-run with --keep) or curl changed shape.
        # Don't silently proceed with an unverified assumption either way.
        if "CURL_RUN_IFELSE" in text and "AC_REQUIRE([AC_RUN_IFELSE])" not in text:
            print(f"  [ok] {target.name} has no AC_REQUIRE([AC_RUN_IFELSE]) -- "
                  f"already patched, nothing to do")
            return
        sys.exit(f"Couldn't find the expected broken line in {target} -- "
                  f"curl's source may have changed shape. Stopping rather than "
                  f"guessing; check {target} manually for CURL_RUN_IFELSE.")

    for i in matched:
        print(f"  removing {target.name}:{i+1}: {lines[i].strip()}")
    new_lines = [line for i, line in enumerate(lines) if i not in matched]
    target.write_text("".join(new_lines))
    print(f"  [ok] patched {target}")


def build_devkitpro_env() -> dict:
    devkitpro = os.environ.get("DEVKITPRO")
    if not devkitpro:
        sys.exit("DEVKITPRO is not set. Source your devkitPro environment first, "
                  "the same way you would before building any devkitPro/libnx "
                  "homebrew project.")
    devkitpro = str(Path(devkitpro))

    devkita64_gcc = Path(devkitpro) / "devkitA64" / "bin" / "aarch64-none-elf-gcc"
    if not devkita64_gcc.exists():
        sys.exit(f"Can't find {devkita64_gcc} -- is devkitA64 installed via (dkp-)pacman?")

    portlibs_prefix = str(Path(devkitpro) / "portlibs" / "switch")
    if not (Path(portlibs_prefix) / "lib" / "libz.a").exists():
        sys.exit(f"Can't find libz.a under {portlibs_prefix}/lib -- "
                  f"install switch-zlib first (dkp-pacman -S switch-zlib).")

    env = os.environ.copy()
    arch = "-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIC -ftls-model=local-exec"
    cflags = f"{arch} -O2 -ffunction-sections -fdata-sections"
    cppflags = f"-D__SWITCH__ -I{portlibs_prefix}/include -isystem{devkitpro}/libnx/include"
    ldflags = f"{arch} -L{portlibs_prefix}/lib -L{devkitpro}/libnx/lib"
    # curl's own PKGBUILD adds the switch.specs bit on top of switchvars.sh's LDFLAGS
    ldflags = f"-specs={devkitpro}/libnx/switch.specs {ldflags}"

    env.update({
        "DEVKITPRO": devkitpro,
        "PORTLIBS_ROOT": str(Path(devkitpro) / "portlibs"),
        "PORTLIBS_PREFIX": portlibs_prefix,
        "PATH": f"{devkitpro}/tools/bin:{devkitpro}/devkitA64/bin:{portlibs_prefix}/bin:{env.get('PATH', '')}",
        "TOOL_PREFIX": "aarch64-none-elf-",
        "CC": str(devkita64_gcc),
        "AR": "aarch64-none-elf-gcc-ar",
        "RANLIB": "aarch64-none-elf-gcc-ranlib",
        "ARCH": arch,
        "CFLAGS": cflags,
        "CXXFLAGS": cflags,
        "CPPFLAGS": cppflags,
        "LDFLAGS": ldflags,
        # Networking (gethostbyname, connect, socket, etc.) on the Switch
        # lives inside libnx.a, not newlib -- switch.specs handles crt/
        # startup wiring but does NOT auto-link libnx, so every devkitPro
        # portlib/homebrew Makefile lists -lnx explicitly. Without this,
        # configure's AC_LINK_IFELSE checks for gethostbyname/connect/etc.
        # can't link their conftest programs and curl's configure aborts
        # with "couldn't find libraries for gethostbyname()". This only
        # affects configure's own link tests -- libcurl.a itself is a
        # static archive and doesn't bake in a dependency on this.
        "LIBS": "-lnx",
        # Belt-and-suspenders: if any other AC_RUN_IFELSE-alike check ever
        # shows up, tell autoconf's generic cache path what a sane default
        # cross-compiling answer is instead of hard-failing. Harmless no-op
        # for checks that don't consult it.
        "cross_compiling": "yes",
    })
    return env


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 4,
                     help="parallel make jobs (default: nproc)")
    ap.add_argument("--no-install", action="store_true",
                     help="build only, skip the sudo install step")
    ap.add_argument("--keep", action="store_true",
                     help="don't wipe an existing extracted source dir before building")
    args = ap.parse_args()

    check_host_tools()
    env = build_devkitpro_env()

    workdir = Path.cwd()
    srcdir = workdir / f"curl-{CURL_VERSION}"
    tarball = workdir / f"curl-{CURL_VERSION}.tar.xz"
    patchfile = workdir / "switch-curl.patch"

    log("Downloading curl source + devkitPro's switch TLS-backend patch")
    download(CURL_URL, tarball, CURL_SHA256)
    download(PATCH_URL, patchfile, PATCH_SHA256)

    if srcdir.exists() and not args.keep:
        log(f"Removing existing {srcdir.name} for a clean build")
        shutil.rmtree(srcdir)

    if not srcdir.exists():
        log("Extracting source")
        run(["tar", "xf", str(tarball)], cwd=workdir)

        log("Applying devkitPro's switch-curl.patch (adds the libnx SSL backend)")
        run(["patch", "-Np1", "-i", str(patchfile)], cwd=srcdir)

        log("Applying upstream fix for curl/curl#5126 (cross-compile AC_REQUIRE bug)")
        fix_cross_compile_bug(srcdir)
    else:
        log(f"{srcdir.name} already exists, reusing (pass without --keep to force a clean rebuild)")
        log("Re-checking cross-compile bug fix is present")
        fix_cross_compile_bug(srcdir)

    log("./buildconf")
    run(["./buildconf"], cwd=srcdir, env=env)

    configure_cmd = [
        "./configure",
        f"--prefix={env['PORTLIBS_PREFIX']}",
        "--host=aarch64-none-elf",
        "--disable-shared", "--enable-static",
        "--disable-ipv6", "--disable-unix-sockets",
        "--disable-manual", "--disable-ntlm-wb", "--disable-threaded-resolver",
        "--without-ssl", "--without-polar-ssl", "--without-cyassl", "--without-wolfssl",
        "--without-mbedtls",
        "--with-libnx",
        "--with-default-ssl-backend=libnx",
    ] + [f"--disable-{proto}" for proto in DISABLE_PROTOCOLS]

    log("./configure (devkitPro's flags + protocol disables)")
    print("Disabling protocols: " + ", ".join(DISABLE_PROTOCOLS))
    run(configure_cmd, cwd=srcdir, env=env)

    log(f"make -C lib -j{args.jobs}")
    run(["make", f"-j{args.jobs}", "-C", "lib"], cwd=srcdir, env=env)

    new_lib = srcdir / "lib" / ".libs" / "libcurl.a"
    if not new_lib.exists():
        # older automake layouts sometimes drop it straight in lib/
        alt = srcdir / "lib" / "libcurl.a"
        new_lib = alt if alt.exists() else new_lib
    if not new_lib.exists():
        sys.exit(f"Build finished but couldn't find libcurl.a under {srcdir/'lib'} -- check the make output above.")

    new_size = new_lib.stat().st_size
    log(f"Built {new_lib} ({new_size:,} bytes)")

    if args.no_install:
        log("--no-install passed, stopping here. Built library is at:")
        print(f"  {new_lib}")
        return

    # --- Back up whatever's currently installed, then install the new build ---
    portlibs_lib = Path(env["PORTLIBS_PREFIX"]) / "lib"
    portlibs_include = Path(env["PORTLIBS_PREFIX"]) / "include" / "curl"
    old_lib = portlibs_lib / "libcurl.a"
    old_size = old_lib.stat().st_size if old_lib.exists() else None

    if old_lib.exists():
        backup_dir = workdir / f"curl-backup-{time.strftime('%Y%m%d-%H%M%S')}"
        backup_dir.mkdir()
        log(f"Backing up currently installed libcurl.a + headers to {backup_dir}")
        shutil.copy2(old_lib, backup_dir / "libcurl.a")
        if portlibs_include.exists():
            shutil.copytree(portlibs_include, backup_dir / "include" / "curl")
        pc = portlibs_lib / "pkgconfig" / "libcurl.pc"
        if pc.exists():
            (backup_dir / "pkgconfig").mkdir(exist_ok=True)
            shutil.copy2(pc, backup_dir / "pkgconfig" / "libcurl.pc")
        print(f"To roll back later: dkp-pacman -S switch-curl (forces a reinstall from the official package)")

    log("Installing (sudo required, same as your libnx install)")
    install_script = (
        "set -e; "
        "make -C lib install; "
        "make -C include install; "
        "make install-binSCRIPTS install-pkgconfigDATA"
    )
    run(["sudo", "-E", "bash", "-c", install_script], cwd=srcdir, env=env)

    # --- Verify ---
    log("Verifying the disabled protocols are actually gone")
    nm = shutil.which("aarch64-none-elf-nm") or str(Path(env["DEVKITPRO"]) / "devkitA64" / "bin" / "aarch64-none-elf-nm")
    installed_lib = portlibs_lib / "libcurl.a"
    nm_out = run([nm, str(installed_lib)], capture=True).stdout

    found = [sym for sym in CANARY_SYMBOLS if sym in nm_out]
    if found:
        print(f"  [!] still present, disables may not have taken effect: {', '.join(found)}")
    else:
        print(f"  [ok] none of {', '.join(CANARY_SYMBOLS)} found in the installed libcurl.a")

    new_installed_size = installed_lib.stat().st_size
    print(f"\n  libcurl.a size: {new_installed_size:,} bytes"
          + (f"  (was {old_size:,} bytes, {old_size - new_installed_size:+,} bytes)" if old_size else ""))

    log("Done. Rebuild your homebrew project normally -- your Makefile is unchanged, it still just links -lcurl.")


if __name__ == "__main__":
    main()
