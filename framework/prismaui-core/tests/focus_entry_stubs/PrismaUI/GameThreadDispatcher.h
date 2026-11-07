#pragma once

#include <cstdint>
#include <functional>

namespace PrismaUI::GameThreadDispatcher {
namespace Test {
bool& Admit();
std::size_t Pending();
void Drain();
void Reset();
}

bool Dispatch(std::function<void()> task, std::uint64_t view = 0);
}
