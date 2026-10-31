#!/usr/bin/env python3
"""Load the built PrismaUI_F4.dll the way the game loader does, and fail loudly if it returns null.

A DLL that compiles and links can still refuse to initialize: the loader runs DllMain and every
static initializer before F4SE ever sees the plugin, and a failure there surfaces only as a null
HMODULE from LoadLibraryW with GetLastError 1114 (ERROR_DLL_INIT_FAILED). That is invisible to a
build, so the load itself has to be exercised.

When --runtime-libs is passed, that directory is prepended to the search path so the provider
resolves its own Ultralight/WebCore/AppCore rather than an ambient copy. Without it, the loader uses
whatever search path the caller already has.

Usage: python .github/scripts/flat-load-smoke.py <path-to-PrismaUI_F4.dll> [--runtime-libs <dir>]
"""

import argparse
import ctypes
import sys
from ctypes import wintypes
from pathlib import Path

ERROR_DLL_INIT_FAILED = 1114


def load_probe(dll: Path, libs: Path | None) -> int:
    if not dll.is_file():
        print(f"flat load smoke failed: {dll} does not exist")
        return 1

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.LoadLibraryW.argtypes = (wintypes.LPCWSTR,)
    kernel32.LoadLibraryW.restype = wintypes.HMODULE
    kernel32.FreeLibrary.argtypes = (wintypes.HMODULE,)
    kernel32.FreeLibrary.restype = wintypes.BOOL

    if libs is not None:
        if not libs.is_dir():
            print(f"flat load smoke failed: runtime libs directory {libs} does not exist")
            return 1
        # Prepend, so the provider resolves its own runtime rather than an ambient copy.
        import os

        os.environ["PATH"] = str(libs) + os.pathsep + os.environ.get("PATH", "")

    ctypes.set_last_error(0)
    handle = kernel32.LoadLibraryW(str(dll))
    if not handle:
        error = ctypes.get_last_error()
        detail = " (ERROR_DLL_INIT_FAILED: provider or dependency initialization failed)" if error == ERROR_DLL_INIT_FAILED else ""
        print(f"flat load smoke failed: LoadLibraryW({dll}) returned NULL, GetLastError={error}{detail}")
        return 1

    ctypes.set_last_error(0)
    if not kernel32.FreeLibrary(handle):
        error = ctypes.get_last_error()
        print(f"flat load smoke failed: FreeLibrary({dll.name}) failed, GetLastError={error}")
        return 1

    print(f"flat load smoke passed: {dll.name} loaded and unloaded (GetLastError=0)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dll", type=Path, help="built PrismaUI_F4.dll to load")
    parser.add_argument("--runtime-libs", type=Path, default=None,
                        help="directory holding the Ultralight runtime DLLs to put on the search path")
    args = parser.parse_args()
    return load_probe(args.dll, args.runtime_libs)


if __name__ == "__main__":
    sys.exit(main())
