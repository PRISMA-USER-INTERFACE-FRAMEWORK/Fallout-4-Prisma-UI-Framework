#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace FreezeDiagnostics {
enum class Lane { ModelPreviewServicing };
enum class Stage { ModelPreviewRead };
struct ScopedStage {
    ScopedStage(Lane, Stage) {}
};
}

namespace logger {
template <class... Args>
void warn(const char*, Args&&...) {}
}

namespace GameThreadDispatcher {
bool Dispatch(std::function<void()> callback);
}
