#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "GPU/LateRerenderPolicy.h"

using namespace PrismaUI::GPU::LateRerender;

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

constexpr std::size_t kMiB = 1024u * 1024u;
constexpr Caps kCaps{4096, 256 * kMiB, 64 * kMiB, 320 * kMiB};

bool SameUsage(const Usage& a, const Usage& b) {
    return a.operations == b.operations && a.textureBytes == b.textureBytes &&
           a.nonTextureBytes == b.nonTextureBytes && a.bytes == b.bytes;
}

Inputs Quiet() {
    Inputs in;
    in.nowMs = 5000;
    return in;
}

}

int main() {
    Check(Saturating(3, 7) == 0, "saturating delta clamps at zero");
    Check(Saturating(9, 7) == 2, "saturating delta counts the increase");
    Check(SameUsage(Delta({10, 1, 2, 3}, {4, 5, 2, 9}), Usage{6, 0, 0, 0}), "delta is per field and saturating");

    DeltaWindow window;
    Check(window.LargestRecorded().operations == 0, "empty window reports zero");
    window.Record(Usage{9, 0, 0, 0});
    for (int i = 0; i < 63; ++i) window.Record(Usage{1, 0, 0, 0});
    Check(window.LargestRecorded().operations == 9, "window keeps 64 renders");
    window.Record(Usage{1, 0, 0, 0});
    Check(window.LargestRecorded().operations == 1, "window evicts the 65th render");

    Backoff backoff;
    backoff.Record(false, 100);
    backoff.Record(false, 100);
    Check(backoff.untilMs == 0, "two misses do not back off");
    backoff.Record(false, 100);
    Check(backoff.untilMs == 100 + kBackoffMs, "third consecutive miss backs off for 1 s");
    Check(backoff.misses == 0, "backoff resets the miss count");
    Backoff reset;
    reset.Record(false, 0);
    reset.Record(false, 0);
    reset.Record(true, 0);
    reset.Record(false, 0);
    reset.Record(false, 0);
    Check(reset.untilMs == 0, "a hit resets consecutive misses");

    Check(Allow(Quiet(), kCaps), "quiet state allows a late re-render");

    Inputs in = Quiet();
    in.pending.operations = 512;
    Check(Allow(in, kCaps), "pending ops at cap/8 is allowed");
    in.pending.operations = 513;
    Check(!Allow(in, kCaps), "pending ops above cap/8 is skipped");

    in = Quiet();
    in.largest.operations = 2048;
    Check(Allow(in, kCaps), "largest delta at cap/2 is allowed");
    in.largest.operations = 2049;
    Check(!Allow(in, kCaps), "overflow case: largest delta above cap/2 is skipped");

    in = Quiet();
    in.largest.operations = 2000;
    in.pending.operations = 48;
    Check(Allow(in, kCaps), "largest plus pending at cap/2 is allowed");
    in.pending.operations = 49;
    Check(!Allow(in, kCaps), "largest plus pending above cap/2 is skipped");

    in = Quiet();
    in.pending.textureBytes = 16 * kMiB;
    Check(Allow(in, kCaps), "pending texture bytes at cap/16 is allowed");
    in.pending.textureBytes = 16 * kMiB + 1;
    Check(!Allow(in, kCaps), "pending texture bytes above cap/16 is skipped");

    in = Quiet();
    in.pending.nonTextureBytes = 4 * kMiB;
    Check(Allow(in, kCaps), "pending non-texture bytes at cap/16 is allowed");
    in.pending.nonTextureBytes = 4 * kMiB + 1;
    Check(!Allow(in, kCaps), "pending non-texture bytes above cap/16 is skipped");

    in = Quiet();
    in.pending.bytes = 20 * kMiB;
    Check(Allow(in, kCaps), "pending total bytes at cap/16 is allowed");
    in.pending.bytes = 20 * kMiB + 1;
    Check(!Allow(in, kCaps), "pending total bytes above cap/16 is skipped");

    in = Quiet();
    in.largest.bytes = 160 * kMiB;
    in.pending.bytes = 0;
    Check(Allow(in, kCaps), "largest total bytes at cap/2 is allowed");
    in.largest.bytes = 160 * kMiB + 1;
    Check(!Allow(in, kCaps), "overflow case: largest total bytes above cap/2 is skipped");

    in = Quiet();
    in.largest.textureBytes = 128 * kMiB;
    Check(Allow(in, kCaps), "largest texture bytes at cap/2 is allowed");
    in.largest.textureBytes = 128 * kMiB + 1;
    Check(!Allow(in, kCaps), "largest texture bytes above cap/2 is skipped");

    in = Quiet();
    in.largest.nonTextureBytes = 32 * kMiB;
    Check(Allow(in, kCaps), "largest non-texture bytes at cap/2 is allowed");
    in.largest.nonTextureBytes = 32 * kMiB + 1;
    Check(!Allow(in, kCaps), "largest non-texture bytes above cap/2 is skipped");

    in = Quiet();
    in.largest.nonTextureBytes = 30 * kMiB;
    in.pending.nonTextureBytes = 2 * kMiB;
    Check(Allow(in, kCaps), "largest plus pending non-texture bytes at cap/2 is allowed");
    in.pending.nonTextureBytes = 2 * kMiB + 1;
    Check(!Allow(in, kCaps), "largest plus pending non-texture bytes above cap/2 is skipped");

    in = Quiet();
    in.structuralChange = true;
    Check(!Allow(in, kCaps), "structural change in flight skips");

    in = Quiet();
    in.spentThisPresent = true;
    Check(!Allow(in, kCaps), "second late re-render in one present interval skips");

    in = Quiet();
    in.backoffUntilMs = 5001;
    Check(!Allow(in, kCaps), "inside backoff skips");
    in.backoffUntilMs = 5000;
    Check(Allow(in, kCaps), "backoff expiry boundary is allowed");

    if (g_failures != 0) {
        std::fprintf(stderr, "late_rerender_policy_test: %d failure(s)\n", g_failures);
        return 1;
    }
    std::puts("late_rerender_policy_test: all checks passed");
    return 0;
}
