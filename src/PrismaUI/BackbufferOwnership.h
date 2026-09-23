#pragma once

#include <cstdint>

namespace PrismaUI::Backbuffer {

    enum class Ownership {
        Unclassified,
        Owned,
        Borrowed,
        Unknown,
    };

    enum class OwnershipOverride { Auto, Owned, Borrowed };

    enum class Reason {
        NotClassified,
        ForcedByUser,
        NoSwapChain,
        NoFrameGen,
        KnownFrameGenGetBufferOwner,
        ForeignGetBufferOwner,
        FrameGenLoadedUnresolved,
    };

    struct Observations {
        bool haveSwapChain = false;

        bool vtableOwnerIsKnownFrameGen = false;

        bool frameGenModuleLoaded = false;

        bool getBufferOwnerForeign = false;
        OwnershipOverride userOverride = OwnershipOverride::Auto;

        bool haveEngineBackbufferSize = false;
        std::uint32_t swapChainDescWidth = 0;
        std::uint32_t swapChainDescHeight = 0;
        std::uint32_t engineBackbufferWidth = 0;
        std::uint32_t engineBackbufferHeight = 0;
    };

    struct Classification {
        Ownership ownership = Ownership::Unclassified;
        Reason    reason = Reason::NotClassified;
    };

    constexpr Classification ClassifyBackbufferOwnership(const Observations& o) {
        if (o.userOverride == OwnershipOverride::Owned) {
            return { Ownership::Owned, Reason::ForcedByUser };
        }
        if (o.userOverride == OwnershipOverride::Borrowed) {
            return { Ownership::Borrowed, Reason::ForcedByUser };
        }
        if (!o.haveSwapChain) {
            return { Ownership::Unknown, Reason::NoSwapChain };
        }

        if (o.vtableOwnerIsKnownFrameGen) {
            return { Ownership::Borrowed, Reason::KnownFrameGenGetBufferOwner };
        }

        if (o.getBufferOwnerForeign) {
            return { Ownership::Unknown, Reason::ForeignGetBufferOwner };
        }

        if (!o.frameGenModuleLoaded) {
            return { Ownership::Owned, Reason::NoFrameGen };
        }

        return { Ownership::Unknown, Reason::FrameGenLoadedUnresolved };
    }

    constexpr bool HasSizeDivergence(const Observations& o) {
        return o.haveEngineBackbufferSize && o.swapChainDescWidth != 0 && o.engineBackbufferWidth != 0 &&
               (o.swapChainDescWidth != o.engineBackbufferWidth ||
                o.swapChainDescHeight != o.engineBackbufferHeight);
    }

    constexpr const char* ToString(Ownership o) {
        switch (o) {
            case Ownership::Unclassified: return "Unclassified";
            case Ownership::Owned:        return "Owned";
            case Ownership::Borrowed:     return "Borrowed";
            default:                      return "Unknown";
        }
    }

    constexpr const char* ToString(Reason r) {
        switch (r) {
            case Reason::ForcedByUser:                 return "forced-by-user";
            case Reason::NoSwapChain:                  return "no-swapchain";
            case Reason::NoFrameGen:                   return "no-framegen";
            case Reason::KnownFrameGenGetBufferOwner:  return "known-fg-getbuffer-owner";
            case Reason::ForeignGetBufferOwner:        return "foreign-getbuffer-owner";
            case Reason::FrameGenLoadedUnresolved: return "framegen-loaded-unresolved";
            default:                                   return "not-classified";
        }
    }

    enum class PresentationMode {
        Owned,
        Borrowed,
        LegacyOwned,
        SafeSkip,
    };

    constexpr PresentationMode ResolveBackbufferPresentationMode(Ownership ownership, const Observations& o) {
        switch (ownership) {
            case Ownership::Owned:    return PresentationMode::Owned;
            case Ownership::Borrowed: return PresentationMode::Borrowed;
            case Ownership::Unknown:

                if (o.getBufferOwnerForeign)     return PresentationMode::SafeSkip;

                if (!o.haveEngineBackbufferSize) return PresentationMode::SafeSkip;

                if (HasSizeDivergence(o))        return PresentationMode::SafeSkip;

                return PresentationMode::LegacyOwned;
            case Ownership::Unclassified:
            default:

                return PresentationMode::SafeSkip;
        }
    }

    constexpr const char* ToString(PresentationMode m) {
        switch (m) {
            case PresentationMode::Owned:       return "Owned";
            case PresentationMode::Borrowed:    return "Borrowed";
            case PresentationMode::LegacyOwned: return "LegacyOwned";
            default:                            return "SafeSkip";
        }
    }

}
