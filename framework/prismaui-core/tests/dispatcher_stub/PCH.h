#pragma once

#include <Windows.h>

namespace logger {
template <class... Args> void info(Args&&...) {}
template <class... Args> void warn(Args&&...) {}
template <class... Args> void error(Args&&...) {}
template <class... Args> void critical(Args&&...) {}
}
