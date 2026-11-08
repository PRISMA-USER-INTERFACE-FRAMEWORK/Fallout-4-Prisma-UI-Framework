// Pure-logic tests for adapter-LUID comparison + the --use-adapter-luid comma encoding (Phase 7).
// g++ -Isrc; no Windows, no D3D. The comma format "HighPart,LowPart" is asserted here; its acceptance
// by the SHIPPED Chromium must still be confirmed at runtime (parser is not in the CEF distribution).

#include "PrismaUI/AdapterLuidPolicy.h"

#include <iostream>
#include <string>

namespace {
int failures = 0;
void Check(bool value, const char* message) {
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
using namespace PrismaUI::AdapterLuidPolicy;
}  // namespace

int main() {
    Check(SameAdapter(LuidValue{5, 7}, LuidValue{5, 7}), "identical LUIDs are the same adapter");
    Check(!SameAdapter(LuidValue{5, 7}, LuidValue{5, 8}), "different HighPart -> different adapter");
    Check(!SameAdapter(LuidValue{5, 7}, LuidValue{6, 7}), "different LowPart -> different adapter");

    Check(IsZero(LuidValue{0, 0}), "{0,0} is zero");
    Check(!IsZero(LuidValue{1, 0}), "{1,0} is non-zero");
    Check(ShouldRequestPin(LuidValue{0x1234, 0}), "real LUID -> request pin");
    Check(!ShouldRequestPin(LuidValue{0, 0}), "zero LUID -> do not request pin");

    // Comma format: HighPart signed int32, LowPart unsigned uint32.
    Check(FormatUseAdapterLuidSwitchValue(LuidValue{2, 1}) == "1,2", "typical LUID -> 'high,low'");
    Check(FormatUseAdapterLuidSwitchValue(LuidValue{0, 0}) == "0,0", "zero LUID -> '0,0'");
    // The critical case: LowPart with bit 31 set must print as a large UNSIGNED value, not negative.
    Check(FormatUseAdapterLuidSwitchValue(LuidValue{0xFFFFFFFF, 0}) == "0,4294967295",
          "LowPart 0xFFFFFFFF prints unsigned (4294967295), not -1");
    // HighPart is signed and may legitimately be negative.
    Check(FormatUseAdapterLuidSwitchValue(LuidValue{7, -1}) == "-1,7", "negative HighPart prints signed");
    Check(FormatUseAdapterLuidSwitchValue(LuidValue{0x80000000, static_cast<std::int32_t>(0x80000000)}) ==
              "-2147483648,2147483648",
          "most-negative HighPart + high-bit LowPart formats correctly");

    // Round-trip through parse.
    auto roundTrips = [](LuidValue v) {
        LuidValue back{};
        return ParseUseAdapterLuidSwitchValue(FormatUseAdapterLuidSwitchValue(v), back) &&
               SameAdapter(v, back);
    };
    Check(roundTrips(LuidValue{0x0000ABCD, 1}), "small LUID round-trips through comma form");
    Check(roundTrips(LuidValue{0xFFFFFFFF, 0}), "high-bit LowPart round-trips");
    Check(roundTrips(LuidValue{0x80000000, -1}), "negative HighPart round-trips");

    // Malformed inputs are rejected, not thrown.
    LuidValue dummy{};
    Check(!ParseUseAdapterLuidSwitchValue("", dummy), "empty string rejected");
    Check(!ParseUseAdapterLuidSwitchValue("123", dummy), "no comma rejected");
    Check(!ParseUseAdapterLuidSwitchValue(",5", dummy), "leading comma rejected");
    Check(!ParseUseAdapterLuidSwitchValue("5,", dummy), "trailing comma rejected");
    Check(!ParseUseAdapterLuidSwitchValue("abc,def", dummy), "non-numeric rejected");
    Check(!ParseUseAdapterLuidSwitchValue("99999999999,0", dummy), "HighPart overflow rejected");
    Check(!ParseUseAdapterLuidSwitchValue("0,99999999999", dummy), "LowPart overflow rejected");

    // The int64 alternate still packs correctly (kept for a possible format switch).
    Check(EncodeUseAdapterLuidInt64(LuidValue{0x00000002, 0x00000001}) == 0x0000000100000002LL,
          "int64 alternate packs HighPart<<32 | LowPart");
    Check(EncodeUseAdapterLuidInt64(LuidValue{0xFFFFFFFF, 0}) == 0x00000000FFFFFFFFLL,
          "int64 alternate: LowPart 0xFFFFFFFF does not sign-extend");

    if (failures != 0) {
        std::cerr << failures << " adapter-luid policy test(s) failed\n";
        return 1;
    }
    std::cout << "adapter-luid policy tests passed\n";
    return 0;
}
