#include "PrismaUI/InvokeResult.h"
#include "PrismaUI/OnceCompletion.h"
#include "PrismaUI/UltralightDispatchQueue.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace PrismaUI::WebRuntimeUltralight;

namespace {
const std::string kStopped = PrismaUI::InvokeResult::Error("web backend stopped before the script ran");

UltralightDispatchQueue::Task ScriptTask(std::vector<std::string>& delivered, bool* ran = nullptr) {
    auto done = std::make_shared<OnceCompletion<std::string>>(
        [&delivered](std::string result) { delivered.push_back(std::move(result)); }, kStopped);
    return [done, ran] {
        if (ran) *ran = true;
        done->Complete("42");
    };
}

UltralightDispatchQueue::Task DomReadyTask(std::vector<std::uint64_t>& ready) {
    auto done = std::make_shared<OnceCompletion<std::uint64_t>>(
        [&ready](std::uint64_t id) { ready.push_back(id); }, 0);
    return [done] { (void)done->Take(); };
}
}  // namespace

int main() {
    {
        std::vector<std::string> delivered;
        UltralightDispatchQueue queue;
        queue.Start();
        queue.Stop();
        bool ran = false;
        const bool admitted = queue.DispatchDeferred(ScriptTask(delivered, &ran));
        assert(!admitted);
        assert(!ran);
        assert(delivered.size() == 1 && delivered[0] == kStopped);
    }

    {
        std::vector<std::string> delivered;
        UltralightDispatchQueue queue;
        queue.Start();
        queue.Stop();
        bool ran = false;
        const bool admitted = queue.Dispatch(ScriptTask(delivered, &ran));
        assert(!admitted);
        assert(!ran);
        assert(delivered.size() == 1 && delivered[0] == kStopped);
    }

    {
        std::vector<std::string> delivered;
        UltralightDispatchQueue queue;
        queue.Start();
        bool ran = false;
        const bool admitted = queue.DispatchDeferred(ScriptTask(delivered, &ran));
        assert(admitted);
        assert(delivered.empty());
        queue.Stop();
        queue.DrainAbandoned();
        assert(!ran);
        assert(delivered.size() == 1 && delivered[0] == kStopped);
        UltralightDispatchQueue::Task remaining;
        const bool popped = queue.TryPop(remaining);
        assert(!popped);
    }

    {
        std::vector<std::string> delivered;
        UltralightDispatchQueue queue;
        queue.Start();
        const bool admitted = queue.DispatchDeferred(ScriptTask(delivered));
        assert(admitted);
        UltralightDispatchQueue::Task task;
        const bool popped = queue.TryPop(task);
        assert(popped);
        task();
        task = nullptr;
        assert(delivered.size() == 1 && delivered[0] == "42");
    }

    {
        std::vector<std::uint64_t> ready;
        UltralightDispatchQueue queue;
        queue.Start();
        const bool admitted = queue.DispatchDeferred(DomReadyTask(ready));
        assert(admitted);
        queue.Stop();
        queue.DrainAbandoned();
        assert(ready.size() == 1 && ready[0] == 0);
    }

    {
        std::vector<std::uint64_t> ready;
        UltralightDispatchQueue queue;
        queue.Start();
        const bool admitted = queue.DispatchDeferred(DomReadyTask(ready));
        assert(admitted);
        UltralightDispatchQueue::Task task;
        const bool popped = queue.TryPop(task);
        assert(popped);
        task();
        task = nullptr;
        assert(ready.empty());
    }

    {
        std::vector<std::string> delivered;
        auto done = std::make_shared<OnceCompletion<std::string>>(
            [&delivered](std::string result) { delivered.push_back(std::move(result)); }, kStopped);
        done->Complete("first");
        done->Complete("second");
        done.reset();
        assert(delivered.size() == 1 && delivered[0] == "first");
    }

    {
        std::vector<std::string> delivered;
        UltralightDispatchQueue queue;
        queue.Start();
        for (int i = 0; i < 3; ++i) assert(queue.DispatchDeferred(ScriptTask(delivered)));
        assert(queue.QueuedCount() == 3);
        queue.RecordPassDepth();
        UltralightDispatchQueue::Task task;
        assert(queue.TryPop(task));
        assert(queue.QueuedCount() == 2);
    }

    std::cout << "script completion tests passed\n";
    return 0;
}
