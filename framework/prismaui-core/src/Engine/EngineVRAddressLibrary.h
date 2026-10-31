#pragma once

// VR address dependency contract; source:docs/code-notes/prismaui-core.md and matrix/vr-address-library.json.
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace PrismaUI::Engine::VRAddressLibrary
{
	inline constexpr std::string_view kMinimumVersion = "1.14.0";
	inline constexpr std::uint64_t kMinimumEntries = 94248;
	inline constexpr std::string_view kInstallFile = "Data/F4SE/plugins/version-1-2-72-0.csv";
	inline constexpr std::array<std::uint64_t, 7> kRequiredIds{ 177801, 343176, 652767, 1582181, 548587, 520890,
		                                                      544871 };

	struct Version
	{
		std::uint32_t major{};
		std::uint32_t minor{};
		std::uint32_t patch{};
	};

	[[nodiscard]] constexpr bool operator<(const Version& a_lhs, const Version& a_rhs) noexcept
	{
		if (a_lhs.major != a_rhs.major) return a_lhs.major < a_rhs.major;
		if (a_lhs.minor != a_rhs.minor) return a_lhs.minor < a_rhs.minor;
		return a_lhs.patch < a_rhs.patch;
	}

	[[nodiscard]] constexpr bool operator==(const Version& a_lhs, const Version& a_rhs) noexcept
	{
		return a_lhs.major == a_rhs.major && a_lhs.minor == a_rhs.minor && a_lhs.patch == a_rhs.patch;
	}

	[[nodiscard]] constexpr std::string_view Trim(std::string_view a_text) noexcept
	{
		while (!a_text.empty() && (a_text.front() == ' ' || a_text.front() == '\t' || a_text.front() == '\r')) {
			a_text.remove_prefix(1);
		}
		while (!a_text.empty() && (a_text.back() == ' ' || a_text.back() == '\t' || a_text.back() == '\r')) {
			a_text.remove_suffix(1);
		}
		return a_text;
	}

	[[nodiscard]] constexpr std::optional<std::uint64_t> ParseUnsigned(std::string_view a_text) noexcept
	{
		if (a_text.empty()) return std::nullopt;
		std::uint64_t value = 0;
		for (const char c : a_text) {
			if (c < '0' || c > '9') return std::nullopt;
			value = value * 10 + static_cast<std::uint64_t>(c - '0');
		}
		return value;
	}

	[[nodiscard]] constexpr std::optional<std::uint64_t> ParseHex(std::string_view a_text) noexcept
	{
		if (a_text.empty() || a_text.size() > 16) return std::nullopt;
		std::uint64_t value = 0;
		for (const char c : a_text) {
			std::uint64_t digit = 0;
			if (c >= '0' && c <= '9') {
				digit = static_cast<std::uint64_t>(c - '0');
			} else if (c >= 'a' && c <= 'f') {
				digit = static_cast<std::uint64_t>(c - 'a' + 10);
			} else if (c >= 'A' && c <= 'F') {
				digit = static_cast<std::uint64_t>(c - 'A' + 10);
			} else {
				return std::nullopt;
			}
			value = value * 16 + digit;
		}
		return value;
	}

	[[nodiscard]] constexpr std::optional<Version> ParseVersion(std::string_view a_text) noexcept
	{
		const auto first = a_text.find('.');
		if (first == std::string_view::npos) return std::nullopt;
		const auto second = a_text.find('.', first + 1);
		if (second == std::string_view::npos) return std::nullopt;
		if (a_text.find('.', second + 1) != std::string_view::npos) return std::nullopt;
		const auto major = ParseUnsigned(a_text.substr(0, first));
		const auto minor = ParseUnsigned(a_text.substr(first + 1, second - first - 1));
		const auto patch = ParseUnsigned(a_text.substr(second + 1));
		if (!major || !minor || !patch || *major > 0xFFFFFFFFull || *minor > 0xFFFFFFFFull || *patch > 0xFFFFFFFFull) {
			return std::nullopt;
		}
		return Version{ static_cast<std::uint32_t>(*major), static_cast<std::uint32_t>(*minor),
			            static_cast<std::uint32_t>(*patch) };
	}

	[[nodiscard]] inline std::string FormatVersion(const Version& a_version)
	{
		return std::to_string(a_version.major) + "." + std::to_string(a_version.minor) + "." +
		       std::to_string(a_version.patch);
	}

	enum class Status
	{
		Ready,
		InvalidMetadata,
		TooOld,
		EntryCountTooLow,
		CoverageIncomplete
	};

	struct Evaluation
	{
		Status status{ Status::InvalidMetadata };
		Version installed{};
		std::uint64_t entries{};
		std::vector<std::uint64_t> missing;
	};

	[[nodiscard]] inline std::string_view Describe(Status a_status) noexcept
	{
		switch (a_status) {
		case Status::Ready:
			return "ready";
		case Status::InvalidMetadata:
			return "the file has an invalid id/offset header, metadata row or data row";
		case Status::TooOld:
			return "the installed release is older than the supported minimum";
		case Status::EntryCountTooLow:
			return "the installed release has fewer entries than the supported minimum";
		case Status::CoverageIncomplete:
			return "the installed release is missing required ids";
		}
		return "unknown";
	}

	[[nodiscard]] inline Evaluation Evaluate(std::string_view a_csv)
	{
		Evaluation result{};
		if (a_csv.size() >= 3 && static_cast<unsigned char>(a_csv[0]) == 0xEF &&
		    static_cast<unsigned char>(a_csv[1]) == 0xBB && static_cast<unsigned char>(a_csv[2]) == 0xBF) {
			a_csv.remove_prefix(3);
		}
		std::size_t cursor = 0;
		auto nextLine = [&]() -> std::string_view {
			if (cursor >= a_csv.size()) return {};
			const auto end = a_csv.find('\n', cursor);
			const auto line = a_csv.substr(cursor, end == std::string_view::npos ? a_csv.size() - cursor : end - cursor);
			cursor = end == std::string_view::npos ? a_csv.size() : end + 1;
			return line;
		};

		std::string_view header;
		while (header.empty()) {
			header = Trim(nextLine());
			if (header.empty() && cursor >= a_csv.size()) return result;
		}
		if (header != "id,offset") return result;

		std::string_view metadata;
		while (metadata.empty()) {
			metadata = Trim(nextLine());
			if (metadata.empty() && cursor >= a_csv.size()) return result;
		}
		const auto comma = metadata.find(',');
		if (comma == std::string_view::npos) return result;
		const auto declaredEntries = ParseUnsigned(Trim(metadata.substr(0, comma)));
		const auto installed = ParseVersion(Trim(metadata.substr(comma + 1)));
		if (!declaredEntries || !installed) return result;
		result.installed = *installed;
		result.entries = *declaredEntries;

		std::vector<std::uint64_t> present;
		while (true) {
			const auto raw = nextLine();
			if (raw.empty() && cursor >= a_csv.size()) break;
			const auto line = Trim(raw);
			if (line.empty()) {
				if (cursor >= a_csv.size()) break;
				continue;
			}
			const auto rowComma = line.find(',');
			if (rowComma == std::string_view::npos) return result;
			const auto id = ParseUnsigned(Trim(line.substr(0, rowComma)));
			if (!id) return result;
			if (!ParseHex(Trim(line.substr(rowComma + 1)))) return result;
			present.push_back(*id);
		}
		if (present.size() != *declaredEntries) return result;
		std::sort(present.begin(), present.end());
		if (std::adjacent_find(present.begin(), present.end()) != present.end()) return result;

		for (const auto id : kRequiredIds) {
			if (!std::binary_search(present.begin(), present.end(), id)) result.missing.push_back(id);
		}

		const auto minimum = ParseVersion(kMinimumVersion);
		if (!minimum) return result;
		if (result.installed < *minimum) {
			result.status = Status::TooOld;
		} else if (result.entries < kMinimumEntries) {
			result.status = Status::EntryCountTooLow;
		} else if (!result.missing.empty()) {
			result.status = Status::CoverageIncomplete;
		} else {
			result.status = Status::Ready;
		}
		return result;
	}
}
