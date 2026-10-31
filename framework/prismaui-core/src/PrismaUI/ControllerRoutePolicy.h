#pragma once

namespace PrismaUI::ControllerInputPolicy {

[[nodiscard]] constexpr bool VirtualPointerEligible(bool nativeMenuReady) noexcept {
    return nativeMenuReady;
}

}
