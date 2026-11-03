#include "PrismaUI/NativeMenuHookInstall.h"

#include <array>
#include <cassert>
#include <map>

namespace {
struct Record {
    bool created = false;
    bool enabled = false;
};

struct FakeMinHook {
    std::map<void*, Record> hooks;
    int failCreateAt = -1;
    int failEnableAt = -1;
    int createCalls = 0;
    int enableCalls = 0;

    static FakeMinHook* current;

    static int Create(void* target, void*, void**) {
        auto& fake = *current;
        if (fake.createCalls++ == fake.failCreateAt) return 1;
        fake.hooks[target].created = true;
        return 0;
    }

    static int Enable(void* target) {
        auto& fake = *current;
        if (fake.enableCalls++ == fake.failEnableAt) return 1;
        fake.hooks[target].enabled = true;
        return 0;
    }

    static int Disable(void* target) {
        auto it = current->hooks.find(target);
        if (it != current->hooks.end()) it->second.enabled = false;
        return 0;
    }

    static int Remove(void* target) {
        current->hooks.erase(target);
        return 0;
    }

    int enabledCount() const {
        int n = 0;
        for (const auto& entry : hooks) {
            if (entry.second.enabled) ++n;
        }
        return n;
    }
};

FakeMinHook* FakeMinHook::current = nullptr;

bool Install(FakeMinHook& fake) {
    FakeMinHook::current = &fake;
    char t0{}, t1{}, t2{}, d0{}, d1{}, d2{};
    void* originals[3]{};
    const std::array<void*, 3> targets{&t0, &t1, &t2};
    const std::array<void*, 3> detours{&d0, &d1, &d2};
    const std::array<void**, 3> originalSlots{&originals[0], &originals[1], &originals[2]};
    return PrismaUI::ControllerGlyphs::InstallNativeMenuHooks(
        targets, detours, originalSlots,
        PrismaUI::ControllerGlyphs::NativeMenuHookOps{
            &FakeMinHook::Create, &FakeMinHook::Enable, &FakeMinHook::Disable, &FakeMinHook::Remove});
}
}

int main() {
    {
        FakeMinHook fake;
        assert(Install(fake));
        assert(fake.hooks.size() == 3);
        assert(fake.enabledCount() == 3);
    }
    for (int failAt = 0; failAt < 3; ++failAt) {
        FakeMinHook fake;
        fake.failEnableAt = failAt;
        assert(!Install(fake));
        assert(fake.enabledCount() == 0);
        assert(fake.hooks.empty());
    }
    for (int failAt = 0; failAt < 3; ++failAt) {
        FakeMinHook fake;
        fake.failCreateAt = failAt;
        assert(!Install(fake));
        assert(fake.enabledCount() == 0);
        assert(fake.hooks.empty());
    }
}
