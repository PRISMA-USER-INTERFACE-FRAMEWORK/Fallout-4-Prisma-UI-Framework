#include "PrismaUI/ThreadObserver.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

namespace {

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main() {
    using PrismaUI::ThreadObserver;
    using Result = ThreadObserver::Result;

    {
        ThreadObserver observer;
        Check(observer.Count() == 0, "a fresh observer has seen no thread");
        Check(!observer.Contains(7), "an unseen id is not contained");
        Check(observer.Observe(7) == Result::NewThread, "the first id is a new thread");
        Check(observer.Observe(7) == Result::Known, "a repeated id is known");
        Check(observer.Contains(7), "a recorded id is contained");
        Check(observer.Count() == 1, "one distinct id so far");
        Check(observer.Observe(0) == Result::Overflow, "id zero is never recorded");
        Check(!observer.Contains(0), "id zero is never contained");
        Check(observer.Count() == 1, "id zero does not change the count");
    }

    {
        ThreadObserver observer;
        for (std::uint32_t id = 1; id <= ThreadObserver::kSlots; ++id) {
            Check(observer.Observe(id) == Result::NewThread, "each of the first four ids is new");
        }
        Check(observer.Observe(ThreadObserver::kSlots + 1) == Result::Overflow, "the fifth distinct id overflows");
        Check(observer.Observe(2) == Result::Known, "a recorded id stays known after overflow");
        Check(!observer.Contains(ThreadObserver::kSlots + 1), "an overflowed id is not contained");
        Check(observer.Count() == ThreadObserver::kSlots, "the count stops at the slot limit");
    }

    {
        ThreadObserver observer;
        std::atomic<int> newThreads{0};
        std::atomic<bool> go{false};
        std::vector<std::thread> threads;
        for (std::uint32_t worker = 0; worker < 8; ++worker) {
            threads.emplace_back([&, worker] {
                while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
                for (int i = 0; i < 2000; ++i) {
                    if (observer.Observe(100 + worker % 3) == Result::NewThread) newThreads.fetch_add(1);
                }
            });
        }
        go.store(true, std::memory_order_release);
        for (auto& thread : threads) thread.join();
        Check(observer.Count() == 3, "a race over three ids records exactly three");
        Check(newThreads.load() == 3, "each id is reported new exactly once");
    }

    Check(&PrismaUI::PresentThreads() == &PrismaUI::PresentThreads(), "present observer is one instance");
    Check(&PrismaUI::PresentThreads() != &PrismaUI::ResizeThreads(), "present and resize observers are separate");

    std::cout << "thread_observer_tests ok\n";
    return 0;
}
