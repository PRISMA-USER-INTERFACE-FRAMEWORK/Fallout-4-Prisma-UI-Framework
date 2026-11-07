#include "PrismaUI_F4_API.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

enum class MockIface : int { None, V1, V8, V9, V10, V11, V12 };

MockIface Provide(PRISMA_UI_API::InterfaceVersion version) {
    using IV = PRISMA_UI_API::InterfaceVersion;
    switch (version) {
    case IV::V1: return MockIface::V1;
    case IV::V8: return MockIface::V8;
    case IV::V9: return MockIface::V9;
    case IV::V10: return MockIface::V10;
    case IV::V11: return MockIface::V11;
    case IV::V12: return MockIface::V12;
    default: return MockIface::None;
    }
}

void expect(bool ok, const char* msg) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        std::exit(1);
    }
}

}

int main() {
    using IV = PRISMA_UI_API::InterfaceVersion;

    expect(static_cast<uint8_t>(IV::V1) == 0x00u, "public header V1 token");
    expect(static_cast<uint8_t>(IV::V8) == 0x07u, "public header V8 token");
    expect(static_cast<uint8_t>(IV::V9) == 0x88u, "public header V9 epoch token");
    expect(static_cast<uint8_t>(IV::V10) == 0x89u, "public header V10 epoch token");
    expect(static_cast<uint8_t>(IV::V11) == 0x8Au, "public header V11 epoch token");
    expect(static_cast<uint8_t>(IV::V12) == 0x8Bu, "public header V12 epoch token");

    expect(Provide(IV::V1) == MockIface::V1, "V1 request maps to V1 interface");
    expect(Provide(IV::V8) == MockIface::V8, "V8 request maps to V8 interface");
    expect(Provide(IV::V9) == MockIface::V9, "new-epoch V9 request maps to V9");
    expect(Provide(IV::V10) == MockIface::V10, "new-epoch V10 request maps to V10");
    expect(Provide(IV::V11) == MockIface::V11, "new-epoch V11 request maps to V11");
    expect(Provide(IV::V12) == MockIface::V12, "new-epoch V12 request maps to V12");

    expect(Provide(static_cast<IV>(8)) == MockIface::None, "stale pre-epoch V9 token 8 fails closed");
    expect(Provide(static_cast<IV>(9)) == MockIface::None, "stale pre-epoch V10 token 9 fails closed");
    expect(Provide(static_cast<IV>(10)) == MockIface::None, "stale pre-epoch V11 token 10 fails closed");
    expect(Provide(static_cast<IV>(11)) == MockIface::None, "stale pre-epoch V12 token 11 fails closed");
    expect(Provide(static_cast<IV>(0x8D)) == MockIface::None, "unknown token 0x8D fails closed");

    std::puts("controller ABI epoch token tests passed");
    return 0;
}