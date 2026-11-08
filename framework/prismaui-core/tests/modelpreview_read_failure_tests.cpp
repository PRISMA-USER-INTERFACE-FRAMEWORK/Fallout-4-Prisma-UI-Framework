#include "modelpreview_reads_test_env.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

std::deque<std::function<void()>> dispatches;
std::mutex dispatchMutex;
std::condition_variable dispatchCv;
bool accepting = true;
std::atomic<bool> rejected = false;

namespace GameThreadDispatcher {
bool Dispatch(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(dispatchMutex);
    if (!accepting) {
        rejected.store(true, std::memory_order_release);
        dispatchCv.notify_one();
        return false;
    }
    dispatches.push_back(std::move(callback));
    dispatchCv.notify_one();
    return true;
}
}

#define PRISMAUI_MODELPREVIEW_READS_TEST
#include "../src/PrismaUI/ModelPreviewReads.cpp"

namespace PrismaUI::ModelPreview {
static bool ReadGameFileOnEngineThread(const std::string& path, std::vector<uint8_t>& out) {
    out.push_back(42);
    if (path == "fail") throw std::runtime_error("injected read failure");
    return true;
}
}

using namespace PrismaUI::ModelPreview;

static bool WaitForDispatch() {
    std::unique_lock<std::mutex> lock(dispatchMutex);
    return dispatchCv.wait_for(lock, std::chrono::seconds(1), [] { return !dispatches.empty(); });
}

static std::function<void()> TakeDispatch() {
    std::lock_guard<std::mutex> lock(dispatchMutex);
    if (dispatches.empty()) return {};
    auto callback = std::move(dispatches.front());
    dispatches.pop_front();
    return callback;
}

static bool WaitForReject() {
    std::unique_lock<std::mutex> lock(dispatchMutex);
    return dispatchCv.wait_for(lock, std::chrono::seconds(1), [] {
        return rejected.load(std::memory_order_acquire);
    });
}

static void DrainDispatches() {
    while (auto callback = TakeDispatch()) {
        try {
            callback();
        } catch (...) {
            assert(false);
        }
    }
}

static bool ReadQueueHasWork() {
    std::lock_guard<std::mutex> lock(g_readMutex);
    return !g_readQueue.empty();
}

int main() {
    std::string failingPath = "fail";
    std::vector<uint8_t> failingBytes;
    bool failingResult = true;
    std::thread failingWorker([&] {
        SetWorkerThread(true);
        failingResult = ReadGameFile(failingPath, failingBytes);
    });
    assert(WaitForDispatch());
    DrainDispatches();
    failingWorker.join();
    assert(!failingResult && failingBytes.empty());
    assert(g_workersWaitingRead.load() == 0);

    std::string successfulPath = "good";
    std::vector<uint8_t> successfulBytes;
    bool successfulResult = false;
    std::thread successfulWorker([&] {
        SetWorkerThread(true);
        successfulResult = ReadGameFile(successfulPath, successfulBytes);
    });
    assert(WaitForDispatch());
    DrainDispatches();
    successfulWorker.join();
    assert(successfulResult && successfulBytes.size() == 1);

    std::vector<uint8_t> retryBytes;
    bool retryResult = true;
    accepting = false;
    std::thread retryWorker([&] {
        SetWorkerThread(true);
        retryResult = ReadGameFile(successfulPath, retryBytes);
    });
    assert(WaitForReject());
    assert(ReadQueueHasWork());
    for (int i = 0; i < 1000 && g_readServiceQueued.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(!g_readServiceQueued.load());
    accepting = true;
    RequestEngineReadService();
    assert(g_readServiceQueued.load());
    DrainDispatches();
    retryWorker.join();
    assert(retryResult && retryBytes.size() == 1);
    assert(!g_readServiceQueued.load());
}
