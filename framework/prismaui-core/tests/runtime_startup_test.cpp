#include "../src/PrismaUI/RuntimeStartupPolicy.h"
#include <cassert>
#include <thread>
#include <vector>

int main() {
    using namespace PrismaUI::RuntimeStartupPolicy;
    using S = PRISMA_UI_FLAT_API::RuntimeState;
    assert(State(false, false, false, false) == S::Waiting);
    assert(State(false, false, true, true) == S::Starting);
    assert(State(false, true, false, true) == S::Starting);
    assert(State(false, true, true, true) == S::Ready);
    assert(State(true, true, true, true) == S::Failed);
    assert(State(false, false, false, true) == S::Starting);
    assert(State(false, true, true, false) == S::Ready);
    assert(State(true, false, false, false) == S::Failed);
    Request request;
    assert(!request.Claim(false));
    assert(!request.Claimed());
    std::atomic<int> starts{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 16; ++i) threads.emplace_back([&] { if (request.Claim(true)) ++starts; });
    for (auto& thread : threads) thread.join();
    assert(starts == 1);
    assert(!request.Claim(true));
}
