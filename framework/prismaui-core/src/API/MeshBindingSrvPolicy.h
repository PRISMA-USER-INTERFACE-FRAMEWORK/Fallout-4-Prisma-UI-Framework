#pragma once

#include <cstdint>

namespace PrismaUI::MeshBinding::SrvPolicy
{
	enum class Restore {
		Original,
		KeepReplaced,
	};

	[[nodiscard]] constexpr Restore DecideRestore(const void* slotNow, const void* weWrote) noexcept
	{
		if (weWrote == nullptr) return Restore::KeepReplaced;
		return slotNow == weWrote ? Restore::Original : Restore::KeepReplaced;
	}

	[[nodiscard]] constexpr bool ConflictsWithOtherView(std::uint64_t ownerView, std::uint64_t candidateView,
	                                                    const void* ownerTexture,
	                                                    const void* candidateTexture) noexcept
	{
		if (candidateTexture == nullptr) return false;
		return ownerTexture == candidateTexture && ownerView != candidateView;
	}
}
