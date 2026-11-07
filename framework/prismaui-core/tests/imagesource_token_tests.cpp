#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "PrismaUI/ImageSourceTokens.h"

namespace {

using PrismaUI::ImageSourceTokens::TokenTable;

constexpr std::uint64_t kThreads = 4;
constexpr std::uint64_t kIterations = 2000;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "imagesource_token_tests: %s\n", message);
        std::abort();
    }
}

bool Resolves(const TokenTable& table, const std::string& path) {
    std::string identifier;
    return table.Resolve(path, identifier);
}

std::string Upper(std::string value) {
    for (auto& c : value) {
        if (c >= 'a' && c <= 'f') c = static_cast<char>(c - 'a' + 'A');
    }
    return value;
}

std::string LetteredPath(TokenTable& table, std::uint64_t key) {
    for (;; ++key) {
        const auto path = table.Reserve(key);
        if (path.substr(11, 16).find_first_of("abcdef") != std::string::npos) return path;
    }
}

}

int main() {
    TokenTable table;
    const auto first = table.Reserve(1);
    const auto second = table.Reserve(2);
    Check(table.Reserve(1) == first, "Reserve must be stable for the same key");
    Check(first != second, "Reserve must return different paths for different keys");
    Check(first.size() == 34 && first.starts_with("/prisma-mp/") && first.ends_with(".imgsrc"),
          "Reserve must return /prisma-mp/<16 hex>.imgsrc");
    const auto token = first.substr(11, 16);
    Check(token.find_first_not_of("0123456789abcdef") == std::string::npos, "the token must be lowercase hex");

    std::string identifier;
    Check(table.Resolve(first, identifier), "a reserved path must resolve");
    Check(identifier == "prisma-mp/" + token, "identifierOut must be prisma-mp/<token>");
    Check(table.Identifier(1) == identifier, "Identifier must match the resolved identifier");
    Check(table.IsReserved(1) && !table.IsReserved(99), "IsReserved must track reserved keys");
    Check(table.Resolve(first.substr(1), identifier), "a path without the leading slash must resolve");
    Check(!table.Resolve("prisma-mp/0000000000000000.imgsrc", identifier), "an unreserved token must not resolve");
    const auto lettered = LetteredPath(table, 3);
    const auto upperToken = Upper(lettered.substr(11, 16));
    Check(Resolves(table, lettered), "a lettered reserved path must resolve");
    Check(!Resolves(table, "prisma-mp/" + upperToken + ".imgsrc"), "an upper-case token must not resolve");
    Check(!Resolves(table, "../" + first.substr(1)), "a leading ../ must not resolve");
    Check(!Resolves(table, "prisma-mp/../" + token + ".imgsrc"), "a ../ segment must not resolve");
    Check(!Resolves(table, "prisma-mp/extra/" + token + ".imgsrc"), "extra segments must not resolve");
    Check(!Resolves(table, "prisma-mp/" + token), "a missing .imgsrc must not resolve");
    Check(!Resolves(table, ""), "an empty path must not resolve");
    Check(!Resolves(table, "/"), "a lone slash must not resolve");

    table.Release(1);
    Check(!Resolves(table, first), "a released path must not resolve");
    Check(table.Identifier(1).empty(), "a released key must have no identifier");
    Check(!table.IsReserved(1), "a released key must not be reserved");
    Check(Resolves(table, table.Reserve(2)), "other keys must stay reserved");

    std::vector<std::thread> workers;
    for (std::uint64_t worker = 0; worker < kThreads; ++worker) {
        workers.emplace_back([&table, worker] {
            for (std::uint64_t i = 0; i < kIterations; ++i) {
                const auto key = 1000 + worker * 16 + (i % 8);
                const auto path = table.Reserve(key);
                Check(Resolves(table, path), "a reserved path must resolve while reserved");
                table.Release(key);
                Check(!Resolves(table, path), "a released path must not resolve");
            }
        });
    }
    for (auto& worker : workers) worker.join();
    return 0;
}
