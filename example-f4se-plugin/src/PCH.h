// example-f4se-plugin/src/PCH.h
#pragma once

#pragma warning(push)
#include <RE/Fallout.h>
#include <F4SE/F4SE.h>
#pragma warning(pop)

using namespace std::literals;

// Logging via REX macros (NewCommonLib):
//   REX::INFO("msg {}", val);
//   REX::WARN("msg {}", val);
//   REX::ERROR("msg {}", val);
// Log file is created automatically by F4SE::Init() at:
//   %USERPROFILE%\Documents\My Games\Fallout4\F4SE\<pluginName>.log

#define DLLEXPORT __declspec(dllexport)
