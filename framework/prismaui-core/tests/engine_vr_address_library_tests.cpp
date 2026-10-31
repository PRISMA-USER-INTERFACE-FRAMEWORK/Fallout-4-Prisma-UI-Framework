// VR address dependency policy tests; source:docs/code-notes/prismaui-core.md.
#include "Engine/EngineVRAddressLibrary.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

using PrismaUI::Engine::VRAddressLibrary::Evaluate;
using PrismaUI::Engine::VRAddressLibrary::Status;
using PrismaUI::Engine::VRAddressLibrary::Version;

std::string Build(const std::string& version, const std::vector<std::uint64_t>& ids,
                  std::uint64_t countOverride = 0, const std::string& header = "id,offset",
                  const std::string& rowSuffix = "") {
    std::string text = header + "\n";
    text += std::to_string(countOverride ? countOverride : ids.size()) + "," + version + "\n";
    for (const auto id : ids) {
        text += std::to_string(id) + "," + "1A2B" + rowSuffix + "\n";
    }
    return text;
}

std::vector<std::uint64_t> Required() {
    return { 177801, 343176, 652767, 1582181, 548587, 520890, 544871 };
}

// Ready fixtures must meet the declared release entry count; source:docs/code-notes/prismaui-core.md.
std::vector<std::uint64_t> Padded(std::vector<std::uint64_t> ids) {
    for (std::uint64_t filler = 2'000'000; ids.size() < PrismaUI::Engine::VRAddressLibrary::kMinimumEntries;
         ++filler) {
        ids.push_back(filler);
    }
    return ids;
}

}  // namespace

int main() {
    using namespace PrismaUI::Engine::VRAddressLibrary;

    Check(ParseVersion(kMinimumVersion) == (Version{ 1, 14, 0 }), "minimum version parses as 1.14.0");
    Check(!ParseVersion("1.14").has_value(), "two-part versions are rejected");
    Check(ParseVersion("1.15.2") == (Version{ 1, 15, 2 }), "three-part versions parse");
    Check(!ParseVersion("1.14.0.1").has_value(), "four-part versions are rejected");

    auto good = Evaluate(Build("1.14.0", Padded(Required())));
    Check(good.status == Status::Ready, "1.14.0 with every required id is ready");
    Check(good.installed == (Version{ 1, 14, 0 }), "installed version is reported");
    Check(good.missing.empty(), "ready result has no missing ids");

    auto newer = Evaluate(Build("1.15.2", Padded(Required())));
    Check(newer.status == Status::Ready, "a newer release is accepted");

    auto older = Evaluate(Build("1.13.1", Required()));
    Check(older.status == Status::TooOld, "1.13.1 is rejected as too old");
    Check(older.installed == (Version{ 1, 13, 1 }), "too-old result reports the installed version");

    auto missing = Evaluate(Build("1.14.0", Padded({ 343176, 652767, 1582181 })));
    Check(missing.status == Status::CoverageIncomplete, "a missing required id is a coverage failure");
    Check(missing.missing.size() == 4 && missing.missing[0] == 177801, "the missing ids are reported");

    auto count = Evaluate(Build("1.14.0", Required(), 10));
    Check(count.status == Status::InvalidMetadata, "a metadata count mismatch is invalid");

    auto lowEntries = Evaluate(Build("1.14.0", Required(), 7));
    Check(lowEntries.status == Status::EntryCountTooLow, "fewer entries than the supported minimum is rejected");

    std::string bom = Build("1.14.0", Required());
    bom.insert(0, "\xEF\xBB\xBF");
    Check(Evaluate(bom).status == Status::EntryCountTooLow, "a UTF-8 BOM is skipped before the header check");

    std::string blankLine = Build("1.14.0", Required(), 7);
    blankLine += "\n";
    Check(Evaluate(blankLine).status == Status::EntryCountTooLow, "a trailing blank line is ignored");

    Check(Evaluate(Build("1.14.0", { 177801, 177801, 343176, 652767, 1582181, 548587, 520890 })).status ==
              Status::InvalidMetadata,
          "duplicate ids are invalid");

    Check(Evaluate("").status == Status::InvalidMetadata, "empty input is invalid");
    Check(Evaluate("id,offset\n").status == Status::InvalidMetadata, "missing metadata is invalid");
    Check(Evaluate("id,offset\nx,1.14.0\n177801,1A2B\n").status == Status::InvalidMetadata,
          "a non-numeric count is invalid");
    Check(Evaluate("id,offset\n1,not-a-version\n177801,1A2B\n").status == Status::InvalidMetadata,
          "a non-version metadata row is invalid");
    Check(Evaluate("id,offset\n1,1.14.0\n177801\n").status == Status::InvalidMetadata,
          "a row without an offset is invalid");
    Check(Evaluate("id,offset\n1,1.14.0\n177801,ZZZZ\n").status == Status::InvalidMetadata,
          "a non-hex offset is invalid");
    Check(Evaluate("offset,id\n1,1.14.0\n177801,1A2B\n").status == Status::InvalidMetadata,
          "a wrong header is invalid");

    auto crlf = Evaluate("id,offset\r\n4,1.14.0\r\n177801,1A2B\r\n343176,1A2B\r\n652767,1A2B\r\n1582181,1A2B\r\n");
    Check(crlf.status == Status::EntryCountTooLow, "CRLF input is parsed and then checked against the minimum");

    std::cout << "engine_vr_address_library_tests ok\n";
    return 0;
}
