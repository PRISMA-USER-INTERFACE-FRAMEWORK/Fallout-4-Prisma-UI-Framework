#include "InputMask.h"
#include "PCH.h"

#include <array>
#include <cstdint>
#include <mutex>

namespace PrismaUI::InputMask {

    namespace {
        using UEF = RE::UserEvents::USER_EVENT_FLAG;
        using OEF = RE::OtherInputEvents::OTHER_EVENT_FLAG;

        RE::BSTSmartPointer<RE::BSInputEnableLayer> g_layer;
        std::mutex g_mutex;
        std::uint8_t g_owners = 0;

        constexpr std::array<UEF, 3> kUserEventFlags{
            UEF::kFighting,
            UEF::kMainFour,
            UEF::kLooking,
        };
        constexpr std::array<OEF, 5> kOtherEventFlags{
            OEF::kPOVChange,
            OEF::kActivation,
            OEF::kJournalTabs,
            OEF::kVATS,
            OEF::kFavorites,
        };

        template <class Enum, std::size_t N>
        [[nodiscard]] constexpr std::uint32_t BuildMask(const std::array<Enum, N>& flags) noexcept
        {
            std::uint32_t mask = 0;
            for (const auto flag : flags) mask |= static_cast<std::uint32_t>(flag);
            return mask;
        }

        constexpr std::uint32_t kUserEventMask = BuildMask(kUserEventFlags);
        constexpr std::uint32_t kOtherEventMask = BuildMask(kOtherEventFlags);

        struct MaskResult {
            bool userOk = false;
            bool otherOk = false;

            bool ok() const noexcept { return userOk && otherOk; }
        };

        MaskResult SetMaskedFlags(bool enable) {
            auto* mgr = RE::BSInputEnableManager::GetSingleton();
            if (!mgr || !g_layer) {
                logger::warn("[PrismaUI InputMask] SetMaskedFlags({}) -- manager or layer unavailable "
                             "(mgr={}, layer={})", enable, mgr != nullptr, g_layer != nullptr);
                return {};
            }

            const auto senderId = RE::UserEvents::SENDER_ID::kScript;
            const std::uint32_t layerId = g_layer->layerID;

            bool userOk = true;
            for (const auto flag : kUserEventFlags) {
                if (!mgr->EnableUserEvent(layerId, flag, enable, senderId)) {
                    userOk = false;
                    logger::warn("[PrismaUI InputMask] EnableUserEvent failed: layerID={} flag={:#x} enable={}",
                                 layerId, static_cast<std::uint32_t>(flag), enable);
                }
            }

            bool otherOk = true;
            for (const auto flag : kOtherEventFlags) {
                if (!mgr->EnableOtherEvent(layerId, flag, enable, senderId)) {
                    otherOk = false;
                    logger::warn("[PrismaUI InputMask] EnableOtherEvent failed: layerID={} flag={:#x} enable={}",
                                 layerId, static_cast<std::uint32_t>(flag), enable);
                }
            }

            const std::uint32_t userAfter = mgr->cachedInputUserEventsFlags.underlying();
            const std::uint32_t otherAfter = mgr->cachedOtherInputEventsFlags.underlying();
            logger::info("[PrismaUI InputMask] SetMaskedFlags enable={} layerID={} "
                         "userEventMask={:#x} (ok={}, cached={:#x}) "
                         "otherEventMask={:#x} (ok={}, cached={:#x})",
                         enable, layerId, kUserEventMask, userOk, userAfter,
                         kOtherEventMask, otherOk, otherAfter);

            if (!enable) {
                const std::uint32_t userStillEnabled = userAfter & kUserEventMask;
                const std::uint32_t otherStillEnabled = otherAfter & kOtherEventMask;
                if (userStillEnabled != 0) {
                    logger::debug("[PrismaUI InputMask] aggregate user-event cache still contains "
                                  "requested bits after per-flag masking: {:#x}; call results remain "
                                  "authoritative", userStillEnabled);
                }
                if (otherStillEnabled != 0) {
                    logger::debug("[PrismaUI InputMask] aggregate other-event cache still contains "
                                  "requested bits after per-flag masking: {:#x}; call results remain "
                                  "authoritative", otherStillEnabled);
                }
            } else {
                logger::debug("[PrismaUI InputMask] release completed; aggregate cached flags may remain "
                              "masked by other layers");
            }
            return {userOk, otherOk};
        }

        std::uint8_t OwnerBits(Owner owner) { return static_cast<std::uint8_t>(owner); }
    }

    bool Acquire(Owner owner) {
        const std::uint8_t ownerBits = OwnerBits(owner);
        if (!ownerBits) return false;

        std::lock_guard lock(g_mutex);
        if ((g_owners & ownerBits) != 0) return true;

        if (g_owners == 0) {
            auto* mgr = RE::BSInputEnableManager::GetSingleton();
            if (!mgr || !mgr->AllocateNewLayer(g_layer, "PrismaUI_F4") || !g_layer) {
                logger::warn("[PrismaUI InputMask] Acquire({}) -- input layer allocation failed",
                             static_cast<unsigned>(ownerBits));
                g_layer.reset();
                return false;
            }
            const auto result = SetMaskedFlags(false);
            if (!result.ok()) {
                (void)SetMaskedFlags(true);
                g_layer.reset();
                logger::warn("[PrismaUI InputMask] Acquire({}) -- rolling back the Prisma input layer",
                             static_cast<unsigned>(ownerBits));
                return false;
            }
            logger::info("[PrismaUI InputMask] applied (layerID={})", g_layer->layerID);
        }

        g_owners = static_cast<std::uint8_t>(g_owners | ownerBits);
        return true;
    }

    bool Release(Owner owner) {
        const std::uint8_t ownerBits = OwnerBits(owner);
        if (!ownerBits) return false;

        std::lock_guard lock(g_mutex);
        if ((g_owners & ownerBits) == 0) return true;
        if ((g_owners & static_cast<std::uint8_t>(~ownerBits)) != 0) {
            g_owners = static_cast<std::uint8_t>(g_owners & ~ownerBits);
            return true;
        }
        const auto result = SetMaskedFlags(true);
        if (!result.ok()) {
            logger::warn("[PrismaUI InputMask] Release({}) -- one or more per-flag restore calls failed; "
                         "dropping the layer", static_cast<unsigned>(ownerBits));
        }

        logger::info("[PrismaUI InputMask] released (layerID={})", g_layer ? g_layer->layerID : 0);
        g_layer.reset();
        g_owners = 0;
        return result.ok();
    }

    bool IsApplied() {
        std::lock_guard lock(g_mutex);
        return g_owners != 0;
    }

}
