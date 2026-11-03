#include "PrismaUI/WebInputWindow.h"

#include <cassert>
#include <thread>

using PrismaUI::WebInput::Detail::WindowState;

static void TestPublishThenClearDropsTheHandle()
{
    WindowState state;
    assert(!state.Installed());
    assert(state.Get() == nullptr);

    int fakeWindow = 0;
    state.Publish(&fakeWindow);
    assert(state.Installed());
    assert(state.Get() == &fakeWindow);

    state.Clear();
    assert(!state.Installed());
    assert(state.Get() == nullptr);
}

static void TestRepublishAfterClear()
{
    WindowState state;
    int first = 0;
    int second = 0;
    state.Publish(&first);
    state.Clear();
    state.Publish(&second);
    assert(state.Installed());
    assert(state.Get() == &second);
    state.Clear();
    assert(state.Get() == nullptr);
}

static void TestPublishNullIsNotInstalled()
{
    WindowState state;
    state.Publish(nullptr);
    assert(!state.Installed());
    assert(state.Get() == nullptr);
}

static void TestConcurrentReadersNeverSeeClearedOrForeignHandles()
{
    WindowState state;
    int fakeWindow = 0;
    state.Publish(&fakeWindow);

    std::thread reader([&state, &fakeWindow] {
        for (int i = 0; i < 200000; ++i) {
            void* handle = state.Get();
            assert(handle == nullptr || handle == &fakeWindow);
        }
    });
    state.Clear();
    reader.join();

    assert(!state.Installed());
    assert(state.Get() == nullptr);
}

static void TestRepublishRaceNeverYieldsAnUnpublishedHandle()
{
    WindowState state;
    int first = 0;
    int second = 0;
    state.Publish(&first);

    std::thread reader([&state, &first, &second] {
        for (int i = 0; i < 200000; ++i) {
            void* handle = state.Get();
            assert(handle == nullptr || handle == &first || handle == &second);
        }
    });
    state.Clear();
    state.Publish(&second);
    reader.join();

    assert(state.Installed());
    assert(state.Get() == &second);
}

static void TestInstalledMatchesHandleInvariant()
{
    WindowState state;
    assert(state.Installed() == (state.Get() != nullptr));

    int window = 0;
    state.Publish(&window);
    assert(state.Installed() == (state.Get() != nullptr));
    assert(state.Get() == &window);

    state.Clear();
    assert(state.Installed() == (state.Get() != nullptr));
    assert(!state.Installed());
}

static void TestClearIsIdempotent()
{
    WindowState state;
    int fakeWindow = 0;
    state.Publish(&fakeWindow);
    state.Clear();
    state.Clear();
    assert(state.Get() == nullptr);
}

int main()
{
    TestPublishThenClearDropsTheHandle();
    TestRepublishAfterClear();
    TestPublishNullIsNotInstalled();
    TestConcurrentReadersNeverSeeClearedOrForeignHandles();
    TestRepublishRaceNeverYieldsAnUnpublishedHandle();
    TestInstalledMatchesHandleInvariant();
    TestClearIsIdempotent();
    return 0;
}
