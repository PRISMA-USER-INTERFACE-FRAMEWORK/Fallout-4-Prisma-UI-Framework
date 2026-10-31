#include "PrismaUI/QueueLatencyStats.h"

#include <chrono>
#include <cstdio>

using PrismaUI::QueueLatency::Snapshot;
using PrismaUI::QueueLatency::Stats;
using Clock = std::chrono::steady_clock;

static int g_failures = 0;

static void expect(bool condition, const char* what)
{
    std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) ++g_failures;
}

static Clock::time_point At(std::chrono::milliseconds ms)
{
    return Clock::time_point(ms);
}

int main()
{
    using std::chrono::milliseconds;

    Stats stats;
    Snapshot due;

    expect(!stats.TakeDue(At(milliseconds(0)), due), "an empty first window reports nothing");

    stats.Record(At(milliseconds(100)), At(milliseconds(103)));
    stats.Record(At(milliseconds(200)), At(milliseconds(207)));
    expect(!stats.TakeDue(At(milliseconds(5000)), due), "a report is not due before the 10 s interval");

    expect(stats.TakeDue(At(milliseconds(10000)), due), "a report is due once the interval has elapsed");
    expect(due.count == 2, "the report counts both recorded waits");
    expect(due.AverageMs() == 5.0, "the report averages the waits (3 ms and 7 ms give 5 ms)");
    expect(due.MaxMs() == 7.0, "the report keeps the largest wait");
    expect(!stats.TakeDue(At(milliseconds(10000)), due), "a second check inside the same interval reports nothing");

    stats.Record(At(milliseconds(10500)), At(milliseconds(10504)));
    expect(!stats.TakeDue(At(milliseconds(15000)), due), "the next window is not due before its interval");
    expect(stats.TakeDue(At(milliseconds(20000)), due), "the next window is due after its own interval");
    expect(due.count == 1 && due.MaxMs() == 4.0,
           "the next window holds only waits recorded after the last report (max reset to 4 ms)");

    expect(!stats.TakeDue(At(milliseconds(30000)), due), "a due window with no waits reports nothing");

    stats.Record(At(milliseconds(30000)), At(milliseconds(30002)));
    stats.Record(At(milliseconds(30000)), At(milliseconds(30001)));
    expect(stats.TakeDue(At(milliseconds(40000)), due) && due.count == 2 && due.MaxMs() == 2.0,
           "the max keeps the largest of several waits, not the last one");

    Stats counts;
    counts.RecordValue(3);
    counts.RecordValue(9);
    expect(counts.TakeDue(At(milliseconds(0)), due) && due.count == 2 && due.sumNs == 12 && due.maxNs == 9,
           "RecordValue keeps a count-typed sum and max with no time conversion");

    using PrismaUI::QueueLatency::JsCallTable;
    using PrismaUI::QueueLatency::SafeName;
    using PrismaUI::QueueLatency::kMaxJsCallNameBytes;

    JsCallTable names;
    expect(&names.Lookup("hudUpdate") == &names.Lookup("hudUpdate"), "a repeated name maps to the same entry");
    for (const char* name : {"a", "b", "c", "d", "e", "f"}) names.Lookup(name);
    expect(names.Lookup("g").name == "g", "an eighth distinct name takes the last named slot");
    expect(names.Lookup("h").name == "(other)", "a ninth distinct name falls into the other bucket");
    expect(names.Lookup("hudUpdate").name == "hudUpdate", "an early name keeps its slot once the table is full");

    JsCallTable plain;
    expect(plain.Lookup("other").name == "other", "a plugin function named other keeps a named slot");

    JsCallTable longNames;
    expect(longNames.Lookup(std::string(kMaxJsCallNameBytes + 1, 'x')).name == "(other)",
           "a name longer than the cap goes to the other bucket");
    expect(longNames.Lookup(std::string(kMaxJsCallNameBytes, 'y')).name.size() == kMaxJsCallNameBytes,
           "a name at the cap keeps its own slot");
    expect(SafeName("ab\ncd\x01") == "ab?cd?", "control characters in a name are replaced before logging");

    if (g_failures == 0) {
        std::printf("\nAll queue-latency-stats assertions passed.\n");
        return 0;
    }

    std::printf("\n%d assertion(s) FAILED.\n", g_failures);
    return 1;
}
