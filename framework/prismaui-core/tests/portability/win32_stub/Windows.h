// Minimal <Windows.h> stand-in, used ONLY to prove the _WIN32 branch of the shipped SDK headers
// still parses on a Linux CI runner. It declares the three things PrismaUI_F4_API.h actually uses
// and nothing else -- if the header starts depending on more of Win32, this stub fails to compile
// and the portability job tells you, instead of the breakage surfacing in a modder's MSVC build.
//
// This is a syntax fixture, not a Win32 implementation. Never link against it.
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef void* HMODULE;
typedef int (*FARPROC)();

extern "C" HMODULE GetModuleHandleW(const wchar_t* moduleName);
extern "C" FARPROC GetProcAddress(HMODULE module, const char* procName);
