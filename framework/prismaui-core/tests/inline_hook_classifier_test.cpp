#include "Hooks/InlineHookClassifier.h"

#include <cstdio>
#include <cstring>
#include <limits>
#include <map>

using namespace PrismaUI::Hooks;

static int g_failures = 0;
static void expect(bool cond, const char* what) {
    std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) ++g_failures;
}

struct FakeEnv {
    std::map<uintptr_t, uint64_t> slots;
    std::map<uintptr_t, std::pair<uintptr_t, std::string>> mods;

    MemoryReader reader() {
        return [this](uintptr_t a, void* out, size_t n) {
            auto it = slots.find(a);
            if (it == slots.end() || n > sizeof(it->second)) return false;
            std::memcpy(out, &it->second, n);
            return true;
        };
    }

    ModuleResolver resolver() {
        return [this](uintptr_t a) -> std::string {
            for (const auto& [base, endAndName] : mods) {
                if (a >= base && a < endAndName.first) return endAndName.second;
            }
            return {};
        };
    }
};

int main() {
    constexpr uintptr_t DXGI = 0x7000'0000;
    constexpr uintptr_t DXGI_END = 0x7001'0000;
    constexpr uintptr_t RTSS = 0x1000'0000;
    constexpr uintptr_t RTSS_END = 0x1001'0000;

    {
        FakeEnv e; e.mods[DXGI] = {DXGI_END, "dxgi.dll"}; e.mods[RTSS] = {RTSS_END, "RTSSHooks64.dll"};
        const uintptr_t code = DXGI + 0x100, dest = RTSS + 0x200;
        const int32_t rel = static_cast<int32_t>(dest - (code + 5));
        uint8_t b[16] = {0xE9}; std::memcpy(b + 1, &rel, 4);
        auto r = ClassifyHookTarget(code, b, sizeof(b), e.reader(), e.resolver());
        expect(r.status == DecodeStatus::InlineJmp && r.isInlineJmp, "E9 classified");
        expect(r.ownerModule == "dxgi.dll", "E9 owner");
        expect(r.jmpTarget == dest && r.jmpTargetModule == "RTSSHooks64.dll", "E9 target");
    }
    {
        FakeEnv e; e.mods[DXGI] = {DXGI_END, "dxgi.dll"}; e.mods[RTSS] = {RTSS_END, "RTSSHooks64.dll"};
        const uintptr_t code = DXGI + 0x100;
        const int32_t disp = 0x4000;
        const uintptr_t slot = code + 6 + disp, dest = RTSS + 0x333;
        e.slots[slot] = dest;
        uint8_t b[16] = {0xFF, 0x25}; std::memcpy(b + 2, &disp, 4);
        auto r = ClassifyHookTarget(code, b, sizeof(b), e.reader(), e.resolver());
        expect(r.status == DecodeStatus::InlineJmp && r.jmpTarget == dest, "FF25 classified");
        expect(r.jmpTargetModule == "RTSSHooks64.dll", "FF25 target module");
    }
    {
        FakeEnv e; e.mods[DXGI] = {DXGI_END, "dxgi.dll"};
        const uintptr_t code = DXGI + 0x100;
        uint8_t b[16] = {0xEB, 0xF9};
        auto r = ClassifyHookTarget(code, b, sizeof(b), e.reader(), e.resolver());
        expect(r.status == DecodeStatus::InlineJmp && r.isInlineJmp, "patch-above short JMP classified");
        expect(r.jmpTarget == code - 5, "patch-above short JMP target");
    }
    {
        FakeEnv e; e.mods[DXGI] = {DXGI_END, "dxgi.dll"}; e.mods[RTSS] = {RTSS_END, "RenamedOverlay.dll"};
        const uint64_t dest = RTSS + 0x10;
        uint8_t b[16] = {0x48, 0xB8}; std::memcpy(b + 2, &dest, 8); b[10] = 0xFF; b[11] = 0xE0;
        auto r = ClassifyHookTarget(DXGI + 0x100, b, sizeof(b), e.reader(), e.resolver());
        expect(r.status == DecodeStatus::InlineJmp && r.jmpTargetModule == "RenamedOverlay.dll", "movabs+jmp classified");
    }
    {
        FakeEnv e; uint8_t b[16] = {0x48, 0x89, 0x5C, 0x24, 0x08};
        auto r = ClassifyHookTarget(DXGI + 0x100, b, sizeof(b), e.reader(), e.resolver());
        expect(r.status == DecodeStatus::Clean && !r.isInlineJmp, "clean prologue");
    }
    {
        FakeEnv e; uint8_t b[3] = {0xE9, 0x01, 0x02};
        auto r = ClassifyHookTarget(DXGI + 0x100, b, sizeof(b), e.reader(), e.resolver());
        expect(r.status == DecodeStatus::Truncated, "truncated E9");
    }
    {
        FakeEnv e; const int32_t disp = 0x4000;
        uint8_t b[16] = {0xFF, 0x25}; std::memcpy(b + 2, &disp, 4);
        auto r = ClassifyHookTarget(DXGI + 0x100, b, sizeof(b), e.reader(), e.resolver());
        expect(r.status == DecodeStatus::IndirectUnreadable, "unreadable FF25 slot");
    }
    {
        FakeEnv e; e.mods[DXGI] = {DXGI_END, "dxgi.dll"};
        const uintptr_t code = DXGI + 0x100;
        const uintptr_t dest = 0x7100'0000;
        const int32_t rel = static_cast<int32_t>(dest - (code + 5));
        uint8_t b[16] = {0xE9}; std::memcpy(b + 1, &rel, 4);
        auto r = ClassifyHookTarget(code, b, sizeof(b), e.reader(), e.resolver());
        expect(r.isInlineJmp && r.jmpTargetModule.empty(), "unknown target module");
    }
    {
        FakeEnv e;
        const int32_t rel = 0x7fffffff;
        uint8_t b[16] = {0xE9}; std::memcpy(b + 1, &rel, 4);
        const uintptr_t code = (std::numeric_limits<uintptr_t>::max)() - 3;
        auto r = ClassifyHookTarget(code, b, sizeof(b), e.reader(), e.resolver());
        expect(r.status == DecodeStatus::BadDisplacement && !r.isInlineJmp, "E9 overflow -> BadDisplacement");
    }
#if UINTPTR_MAX > 0xFFFFFFFFu
    {
        FakeEnv e;
        const int32_t disp = 0x1000;
        uint8_t b[16] = {0xFF, 0x25}; std::memcpy(b + 2, &disp, 4);
        const uintptr_t code = 0x0000'7fff'ffff'f800ull;
        auto r = ClassifyHookTarget(code, b, sizeof(b), e.reader(), e.resolver());
        expect(r.status == DecodeStatus::BadDisplacement, "FF25 non-canonical slot -> BadDisplacement");
    }
    {
        FakeEnv e;
        const int32_t disp = 0x100;
        const uintptr_t code = DXGI + 0x100;
        const uintptr_t slot = code + 6 + disp;
        e.slots[slot] = 0x0000'8000'0000'0000ull;
        uint8_t b[16] = {0xFF, 0x25}; std::memcpy(b + 2, &disp, 4);
        auto r = ClassifyHookTarget(code, b, sizeof(b), e.reader(), e.resolver());
        expect(r.status == DecodeStatus::BadDisplacement, "FF25 non-canonical target -> BadDisplacement");
    }
    {
        FakeEnv e;
        const uint64_t dest = 0x0000'8000'0000'0000ull;
        uint8_t b[16] = {0x48, 0xB8}; std::memcpy(b + 2, &dest, 8); b[10] = 0xFF; b[11] = 0xE0;
        auto r = ClassifyHookTarget(DXGI + 0x100, b, sizeof(b), e.reader(), e.resolver());
        expect(r.status == DecodeStatus::BadDisplacement, "movabs non-canonical target -> BadDisplacement");
    }
#endif

    if (g_failures == 0) {
        std::printf("\nAll classifier assertions passed.\n");
        return 0;
    }
    std::printf("\n%d assertion(s) FAILED.\n", g_failures);
    return 1;
}
