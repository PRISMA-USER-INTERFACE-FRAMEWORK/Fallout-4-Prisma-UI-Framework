#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace PrismaUI::ViewLifecycle {

    [[nodiscard]] std::int64_t NowUs() noexcept;

    void NoteRequested(std::uint64_t view, std::string_view path, std::string_view owner, bool staggered,
                       std::int64_t requestUs);
    void NoteExecuted(std::uint64_t view, std::int64_t nowUs);
    void NoteCreated(std::uint64_t view, std::int64_t nowUs);
    void NoteDomReady(std::uint64_t view, std::int64_t nowUs);
    void NoteFirstPublish(std::uint64_t view, std::int64_t nowUs);
    void NoteGone(std::uint64_t view, const char* how);

    [[nodiscard]] bool BurstReadyToClose(std::int64_t nowUs);
    [[nodiscard]] std::string TakeBurstSummary(std::int64_t nowUs);

}
