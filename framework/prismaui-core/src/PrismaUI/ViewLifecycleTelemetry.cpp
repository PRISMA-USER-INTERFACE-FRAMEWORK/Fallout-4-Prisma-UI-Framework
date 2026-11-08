#include "PCH.h"
#include "ViewLifecycleTelemetry.h"

#include "ViewLifecycleBurst.h"

#include <chrono>
#include <cstdio>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace PrismaUI::ViewLifecycle {

    namespace {

        constexpr std::int64_t kBurstMaxWaitUs = 10'000'000;

        struct Timeline {
            std::string path;
            std::string owner;
            std::size_t burstSize = 0;
            bool staggered = false;
            std::int64_t requestUs = 0;
            std::int64_t executedUs = 0;
            std::int64_t createdUs = 0;
            std::int64_t domReadyUs = 0;
            std::int64_t firstPublishUs = 0;
        };

        struct Report {
            std::uint64_t view = 0;
            std::string status;
            std::string owner;
            std::string path;
            std::size_t burstSize = 0;
            bool staggered = false;
            std::string queue;
            std::string create;
            std::string domReady;
            std::string firstPublish;
        };

        std::mutex g_mutex;
        std::unordered_map<std::uint64_t, Timeline> g_timelines;
        Burst g_burst;

        std::string Span(std::int64_t from, std::int64_t to) {
            if (!from || !to || to < from) return "-";
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2fms", static_cast<double>(to - from) / 1000.0);
            return buf;
        }

        std::string Offset(std::int64_t requestUs, std::int64_t at) {
            if (!requestUs || !at || at < requestUs) return "-";
            char buf[32];
            std::snprintf(buf, sizeof(buf), "+%.2fms", static_cast<double>(at - requestUs) / 1000.0);
            return buf;
        }

        Report MakeReport(std::uint64_t view, const Timeline& t, const char* status) {
            Report r;
            r.view = view;
            r.status = status ? status : "gone";
            r.owner = t.owner;
            r.path = t.path;
            r.burstSize = t.burstSize;
            r.staggered = t.staggered;
            r.queue = t.executedUs ? Span(t.requestUs, t.executedUs) : "-";
            r.create = t.createdUs ? Span(t.executedUs, t.createdUs) : "-";
            r.domReady = Offset(t.requestUs, t.domReadyUs);
            r.firstPublish = Offset(t.requestUs, t.firstPublishUs);
            return r;
        }

        void Emit(const Report& r) {
            logger::info("[ViewLifecycle] view={} status={} owner='{}' path='{}' burst={} staggered={} "
                         "request->executed={} create={} domReady={} firstPublish={}",
                         r.view, r.status, r.owner, r.path, r.burstSize, r.staggered ? 1 : 0, r.queue, r.create,
                         r.domReady, r.firstPublish);
        }

        std::optional<Report> TakeIfComplete(std::uint64_t view) {
            const auto it = g_timelines.find(view);
            if (it == g_timelines.end()) return std::nullopt;
            if (!it->second.domReadyUs || !it->second.firstPublishUs) return std::nullopt;
            Report r = MakeReport(view, it->second, "ready");
            g_timelines.erase(it);
            return r;
        }

    }

    std::int64_t NowUs() noexcept {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    void NoteRequested(std::uint64_t view, std::string_view path, std::string_view owner, bool staggered,
                       std::int64_t requestUs) {
        std::lock_guard lock(g_mutex);
        const std::size_t size = g_burst.Request(view, requestUs);
        Timeline& t = g_timelines[view];
        t = Timeline{};
        t.path = std::string(path);
        t.owner = std::string(owner);
        t.burstSize = size;
        t.staggered = staggered;
        t.requestUs = requestUs;
    }

    void NoteExecuted(std::uint64_t view, std::int64_t nowUs) {
        std::lock_guard lock(g_mutex);
        const auto it = g_timelines.find(view);
        if (it != g_timelines.end()) it->second.executedUs = nowUs;
    }

    void NoteCreated(std::uint64_t view, std::int64_t nowUs) {
        std::lock_guard lock(g_mutex);
        const auto it = g_timelines.find(view);
        if (it == g_timelines.end() || !it->second.executedUs || it->second.createdUs) return;
        it->second.createdUs = nowUs;
        if (nowUs >= it->second.executedUs)
            g_burst.CreateDone(static_cast<std::uint64_t>(nowUs - it->second.executedUs));
    }

    void NoteDomReady(std::uint64_t view, std::int64_t nowUs) {
        std::optional<Report> ready;
        {
            std::lock_guard lock(g_mutex);
            const auto it = g_timelines.find(view);
            if (it == g_timelines.end() || it->second.domReadyUs) return;
            it->second.domReadyUs = nowUs;
            g_burst.DomReady(view, nowUs);
            ready = TakeIfComplete(view);
        }
        if (ready) Emit(*ready);
    }

    void NoteFirstPublish(std::uint64_t view, std::int64_t nowUs) {
        std::optional<Report> ready;
        {
            std::lock_guard lock(g_mutex);
            const auto it = g_timelines.find(view);
            if (it == g_timelines.end() || it->second.firstPublishUs) return;
            it->second.firstPublishUs = nowUs;
            ready = TakeIfComplete(view);
        }
        if (ready) Emit(*ready);
    }

    void NoteGone(std::uint64_t view, const char* how) {
        std::optional<Report> gone;
        {
            std::lock_guard lock(g_mutex);
            g_burst.Abandon(view);
            const auto it = g_timelines.find(view);
            if (it != g_timelines.end()) {
                gone = MakeReport(view, it->second, how);
                g_timelines.erase(it);
            }
        }
        if (gone) Emit(*gone);
    }

    bool BurstReadyToClose(std::int64_t nowUs) {
        std::lock_guard lock(g_mutex);
        return g_burst.ReadyToClose(nowUs, kBurstMaxWaitUs);
    }

    std::string TakeBurstSummary(std::int64_t nowUs) {
        (void)nowUs;
        std::lock_guard lock(g_mutex);
        if (!g_burst.Open()) return {};
        const BurstSummary s = g_burst.Take();
        char buf[320];
        const std::string firstToLastDomReady =
            s.domReady && s.lastDomReadyUs >= s.firstRequestUs ? Span(s.firstRequestUs, s.lastDomReadyUs)
                                                               : std::string("-");
        std::snprintf(buf, sizeof(buf),
                      "viewBurst views=%zu domReady=%zu abandoned=%zu outstanding=%zu "
                      "create total=%.2fms max=%.2fms mean=%.2fms firstRequestToLastDomReady=%s",
                      s.views, s.domReady, s.abandoned, s.outstanding,
                      static_cast<double>(s.createTotalUs) / 1000.0, static_cast<double>(s.createMaxUs) / 1000.0,
                      s.MeanCreateUs() / 1000.0, firstToLastDomReady.c_str());
        return buf;
    }

}
