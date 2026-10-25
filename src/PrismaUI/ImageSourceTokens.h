#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <string_view>

namespace PrismaUI::ImageSourceTokens {

    inline constexpr std::string_view kIdentifierPrefix = "prisma-mp/";
    inline constexpr std::string_view kLogicalSuffix = ".imgsrc";
    inline constexpr std::size_t kTokenLength = 16;

    [[nodiscard]] inline bool IsToken(std::string_view token) noexcept {
        if (token.size() != kTokenLength) return false;
        for (const char c : token) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        }
        return true;
    }

    [[nodiscard]] inline std::string FormatToken(std::uint64_t value) {
        constexpr std::string_view digits = "0123456789abcdef";
        std::string token(kTokenLength, '0');
        for (std::size_t index = kTokenLength; index-- > 0; value >>= 4) token[index] = digits[value & 0xfu];
        return token;
    }

    [[nodiscard]] inline bool ParseLogicalPath(std::string_view path, std::string_view& token) noexcept {
        if (path.starts_with('/')) path.remove_prefix(1);
        if (path.size() != kIdentifierPrefix.size() + kTokenLength + kLogicalSuffix.size()) return false;
        if (!path.starts_with(kIdentifierPrefix) || !path.ends_with(kLogicalSuffix)) return false;
        token = path.substr(kIdentifierPrefix.size(), kTokenLength);
        return IsToken(token);
    }

    class TokenTable final {
    public:
        [[nodiscard]] std::string Reserve(std::uint64_t key) {
            std::lock_guard lock{mutex_};
            auto found = tokenByKey_.find(key);
            if (found == tokenByKey_.end()) {
                std::random_device random;
                std::string token;
                do {
                    const auto high = static_cast<std::uint64_t>(random());
                    token = FormatToken((high << 32) | random());
                } while (reserved_.contains(token));
                found = tokenByKey_.emplace(key, token).first;
                reserved_.insert(token);
            }
            return "/" + std::string(kIdentifierPrefix) + found->second + std::string(kLogicalSuffix);
        }

        [[nodiscard]] std::string Identifier(std::uint64_t key) const {
            std::lock_guard lock{mutex_};
            const auto found = tokenByKey_.find(key);
            return found == tokenByKey_.end() ? std::string{} : std::string(kIdentifierPrefix) + found->second;
        }

        [[nodiscard]] bool IsReserved(std::uint64_t key) const {
            std::lock_guard lock{mutex_};
            return tokenByKey_.contains(key);
        }

        void Release(std::uint64_t key) {
            std::lock_guard lock{mutex_};
            const auto found = tokenByKey_.find(key);
            if (found == tokenByKey_.end()) return;
            reserved_.erase(found->second);
            tokenByKey_.erase(found);
        }

        void Clear() {
            std::lock_guard lock{mutex_};
            tokenByKey_.clear();
            reserved_.clear();
        }

        [[nodiscard]] bool Resolve(std::string_view logicalPath, std::string& identifierOut) const {
            std::string_view token;
            if (!ParseLogicalPath(logicalPath, token)) return false;
            std::lock_guard lock{mutex_};
            if (!reserved_.contains(std::string(token))) return false;
            identifierOut = std::string(kIdentifierPrefix) + std::string(token);
            return true;
        }

    private:
        mutable std::mutex mutex_;
        std::map<std::uint64_t, std::string> tokenByKey_;
        std::set<std::string> reserved_;
    };

}
