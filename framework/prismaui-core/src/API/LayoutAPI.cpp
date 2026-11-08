#include "API.h"
#include "LayoutHost.h"

#include "PrismaUI/LayoutState.h"
#include "PrismaUI/WebRuntime.h"
#include "Utils/ConflictChecker.h"

#include <KnownFolders.h>
#include <ShlObj.h>
#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace PrismaUI::LayoutHost {
    namespace {
        namespace L = PrismaUI::Layout;

        constexpr std::size_t kMaxSubscribers = 32;
        constexpr std::string_view kSystemOwner = "PrismaUI";

        enum ChangeKind : std::uint32_t {
            kRect = 1,
            kVisibility = 2,
            kReset = 3,
            kTracked = 4,
            kGone = 5,
            kEditMode = 6,
            kLoaded = 7
        };

        struct Subscriber {
            std::uint32_t token = 0;
            Flat::LayoutChangeCallback callback = nullptr;
            void* userdata = nullptr;
        };

        std::mutex g_subscriberMutex;
        std::vector<Subscriber> g_subscribers;
        std::uint32_t g_nextToken = 1;
        std::recursive_mutex g_deliveryMutex;
        std::mutex g_fileMutex;
        std::atomic<bool> g_loaded{false};
        std::atomic<bool> g_loadRejected{false};

        Flat::LayoutRect ToFlat(const L::Rect& rect) { return {rect.x, rect.y, rect.w, rect.h}; }
        L::Rect FromFlat(const Flat::LayoutRect& rect) { return {rect.x, rect.y, rect.width, rect.height}; }

        std::filesystem::path LayoutFilePath() {
            PWSTR documents = nullptr;
            if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documents))) return {};
            std::filesystem::path path{documents};
            CoTaskMemFree(documents);
            return path / L"My Games" / L"Fallout4" / L"PrismaUI_F4" / L"layout.json";
        }

        std::string TitleFromPath(std::string_view path, std::string_view owner) {
            std::string title;
            const auto marker = path.find("Interface/");
            if (marker != std::string_view::npos) {
                const auto start = marker + std::string_view{"Interface/"}.size();
                const auto end = path.find('/', start);
                title.assign(path.substr(start, end == std::string_view::npos ? path.size() - start : end - start));
            }
            if (title.empty()) title.assign(owner);
            if (title.size() > 4 && title.ends_with(".dll")) title.erase(title.size() - 4);
            return title.empty() ? std::string{path} : title;
        }

        constexpr std::string_view kUnresolvedOwner = "<unknown>";

        std::string CallerOwner(void* caller) {
            if (!caller) return {};
            std::string who = PrismaUI::ConflictChecker::OwnerOf(caller);
            return who == kUnresolvedOwner ? std::string{} : who;
        }

        bool OwnsViewAs(const L::Entry& entry, const std::string& who) {
            return !who.empty() && !entry.owner.empty() && PrismaUI::ConflictChecker::EqualsIgnoreCase(who, entry.owner);
        }

        bool OwnsView(const L::Entry& entry, void* caller) { return OwnsViewAs(entry, CallerOwner(caller)); }

        bool StillSubscribed(std::uint32_t token) {
            std::lock_guard lock{g_subscriberMutex};
            return std::any_of(g_subscribers.begin(), g_subscribers.end(),
                               [token](const Subscriber& s) { return s.token == token; });
        }

        bool ToViewId(double value, std::uint64_t& out) noexcept {
            if (!(value >= 0.0) || value > 9.0e15) return false;
            out = static_cast<std::uint64_t>(value);
            return true;
        }

        void AppendRect(std::string& out, const L::Rect& rect) {
            out += '[';
            L::AppendNumber(out, rect.x);
            out += ',';
            L::AppendNumber(out, rect.y);
            out += ',';
            L::AppendNumber(out, rect.w);
            out += ',';
            L::AppendNumber(out, rect.h);
            out += ']';
        }

        std::string BuildEditorPayload() {
            const auto settings = L::Instance().GetSettings();
            std::string out = "{\"snap\":" + std::to_string(settings.snap) + ",\"grid\":" +
                              (settings.grid ? "true" : "false") + ",\"views\":[";
            bool first = true;
            for (const auto& [view, entry] : L::Instance().List()) {
                if (WebRuntime::IsOffscreen(view) || !WebRuntime::IsValid(view)) continue;
                if (!first) out += ',';
                first = false;
                out += "{\"id\":" + std::to_string(view) + ",\"title\":";
                L::AppendEscaped(out, entry.title);
                out += ",\"owner\":";
                L::AppendEscaped(out, entry.owner);
                out += ",\"registered\":" + std::string{entry.registered ? "true" : "false"} +
                       ",\"hidden\":" + (entry.hidden ? "true" : "false") + ",\"rect\":";
                AppendRect(out, entry.rect);
                out += ",\"default\":";
                AppendRect(out, entry.defaultRect);
                out += '}';
            }
            out += "]}";
            return out;
        }

        void PushEditorState() {
            if (!L::Instance().EditMode()) return;
            const auto editor = WebRuntime::LayoutEditorView();
            if (editor && WebRuntime::IsValid(editor)) WebRuntime::InteropCall(editor, "setLayout", BuildEditorPayload());
        }

        void Notify(std::uint32_t kind, std::uint64_t view, bool pushEditor = true) {
            if (pushEditor) PushEditorState();
            std::vector<Subscriber> subscribers;
            {
                std::lock_guard lock{g_subscriberMutex};
                subscribers = g_subscribers;
            }
            if (subscribers.empty()) return;
            (void)DispatchApiCallback([subscribers = std::move(subscribers), kind, view] {
                Flat::LayoutChange change{static_cast<std::uint32_t>(sizeof(Flat::LayoutChange)), kind, view};
                for (const auto& subscriber : subscribers) {
                    std::lock_guard delivery{g_deliveryMutex};
                    if (subscriber.callback && StillSubscribed(subscriber.token))
                        subscriber.callback(&change, subscriber.userdata);
                }
            });
        }

        void NotifyEditMode(bool editing) {
            const std::string script =
                std::string{"window.dispatchEvent(new CustomEvent('prisma-layout-edit',{detail:{editing:"} +
                (editing ? "true" : "false") + "}}))";
            for (const auto& [view, entry] : L::Instance().List())
                if (entry.registered && WebRuntime::IsValid(view)) WebRuntime::Invoke(view, script, {});
        }

        bool ReadFile(const std::filesystem::path& path, std::string& out) {
            std::error_code ec;
            const auto size = std::filesystem::file_size(path, ec);
            if (ec || size > L::kMaxFileBytes) return false;
            std::ifstream stream(path, std::ios::binary);
            if (!stream) return false;
            out.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
            return true;
        }

        Flat::LayoutResult LoadFromDisk() {
            const auto path = LayoutFilePath();
            std::error_code ec;
            if (path.empty() || !std::filesystem::exists(path, ec)) return Flat::LayoutResult::IoFailed;
            std::string text;
            if (!ReadFile(path, text) || !L::Instance().Load(text)) {
                g_loadRejected.store(true, std::memory_order_release);
                return Flat::LayoutResult::IoFailed;
            }
            g_loadRejected.store(false, std::memory_order_release);
            return Flat::LayoutResult::Ok;
        }

        void EnsureLoaded() {
            if (g_loaded.load(std::memory_order_acquire)) return;
            std::lock_guard lock{g_fileMutex};
            if (g_loaded.load(std::memory_order_acquire)) return;
            if (LoadFromDisk() == Flat::LayoutResult::Ok) logger::info("[Layout] loaded layout file");
            g_loaded.store(true, std::memory_order_release);
        }

        bool SplitNumbers(std::string_view text, double* out, std::size_t count) {
            for (std::size_t i = 0; i < count; ++i) {
                const auto comma = text.find(',');
                const auto token = text.substr(0, comma);
                const auto result = std::from_chars(token.data(), token.data() + token.size(), out[i]);
                if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) return false;
                if (comma == std::string_view::npos) return i + 1 == count;
                text.remove_prefix(comma + 1);
            }
            return false;
        }

        bool ResolveTracked(PrismaView view, L::Entry& entry) { return view && L::Instance().Get(view, entry); }
    }

    void OnViewCreated(std::uint64_t view, const std::string& path, const std::string& owner) {
        if (!view || owner == kSystemOwner) return;
        EnsureLoaded();
        const std::string recorded = owner == kUnresolvedOwner ? std::string{} : owner;
        L::Instance().Track(view, L::NormalizeKey(path), recorded, TitleFromPath(path, recorded));
        Notify(kTracked, view);
    }

    void OnViewDestroyed(std::uint64_t view) {
        L::Instance().Untrack(view);
        Notify(kGone, view);
    }

    void OnDockHidden() {
        if (L::Instance().EditMode()) (void)SetEditMode(false);
    }

    void RequestEditMode(bool enabled) {
        (void)DispatchApiCallback([enabled] { (void)SetEditMode(enabled); });
    }

    Flat::LayoutResult RegisterView(PrismaView view, const char* title, const Flat::LayoutRect* defaultRect,
                                    void* caller) noexcept {
        EnsureLoaded();
        L::Entry entry;
        if (!ResolveTracked(view, entry)) return Flat::LayoutResult::UnknownView;
        if (!OwnsView(entry, caller)) return Flat::LayoutResult::NotOwner;
        L::Rect rect;
        if (defaultRect) {
            rect = FromFlat(*defaultRect);
            if (!L::IsFinite(rect)) return Flat::LayoutResult::InvalidArgument;
        }
        if (!L::Instance().Register(view, title ? title : "", defaultRect ? &rect : nullptr))
            return Flat::LayoutResult::UnknownView;
        Notify(kTracked, view);
        return Flat::LayoutResult::Ok;
    }

    Flat::LayoutResult UnregisterView(PrismaView view, void* caller) noexcept {
        L::Entry entry;
        if (!ResolveTracked(view, entry)) return Flat::LayoutResult::UnknownView;
        if (!OwnsView(entry, caller)) return Flat::LayoutResult::NotOwner;
        (void)L::Instance().Unregister(view);
        Notify(kTracked, view);
        return Flat::LayoutResult::Ok;
    }

    Flat::LayoutResult EnumerateViews(Flat::LayoutViewCallback callback, void* userdata, void* caller) noexcept {
        if (!callback) return Flat::LayoutResult::InvalidArgument;
        const std::string who = CallerOwner(caller);
        for (const auto& [view, entry] : L::Instance().List()) {
            if (WebRuntime::IsOffscreen(view) || !WebRuntime::IsValid(view)) continue;
            Flat::LayoutViewInfo info{};
            info.structSize = static_cast<std::uint32_t>(sizeof(info));
            info.view = view;
            info.title = entry.title.c_str();
            info.owner = entry.owner.c_str();
            info.path = entry.key.c_str();
            info.rect = ToFlat(entry.rect);
            info.defaultRect = ToFlat(entry.defaultRect);
            info.flags = (entry.hidden ? Flat::LayoutFlagUserHidden : Flat::LayoutFlagShown) |
                         (entry.registered ? Flat::LayoutFlagRegistered : 0u) |
                         (OwnsViewAs(entry, who) ? Flat::LayoutFlagOwnedByCaller : 0u);
            callback(&info, userdata);
        }
        return Flat::LayoutResult::Ok;
    }

    Flat::LayoutResult GetRect(PrismaView view, Flat::LayoutRect* out) noexcept {
        if (!out) return Flat::LayoutResult::InvalidArgument;
        L::Entry entry;
        if (!ResolveTracked(view, entry)) return Flat::LayoutResult::UnknownView;
        *out = ToFlat(entry.rect);
        return Flat::LayoutResult::Ok;
    }

    Flat::LayoutResult SetRect(PrismaView view, const Flat::LayoutRect* rect, void* caller) noexcept {
        if (!rect || !L::IsFinite(FromFlat(*rect))) return Flat::LayoutResult::InvalidArgument;
        L::Entry entry;
        if (!ResolveTracked(view, entry)) return Flat::LayoutResult::UnknownView;
        if (!OwnsView(entry, caller)) return Flat::LayoutResult::NotOwner;
        if (!L::Instance().SetRect(view, FromFlat(*rect))) return Flat::LayoutResult::UnknownView;
        Notify(kRect, view);
        return Flat::LayoutResult::Ok;
    }

    Flat::LayoutResult SetUserVisible(PrismaView view, bool visible, void* caller) noexcept {
        L::Entry entry;
        if (!ResolveTracked(view, entry)) return Flat::LayoutResult::UnknownView;
        if (!OwnsView(entry, caller)) return Flat::LayoutResult::NotOwner;
        if (!L::Instance().SetHidden(view, !visible)) return Flat::LayoutResult::UnknownView;
        Notify(kVisibility, view);
        return Flat::LayoutResult::Ok;
    }

    Flat::LayoutResult ResetView(PrismaView view, void* caller) noexcept {
        L::Entry entry;
        if (!ResolveTracked(view, entry)) return Flat::LayoutResult::UnknownView;
        if (!OwnsView(entry, caller)) return Flat::LayoutResult::NotOwner;
        if (!L::Instance().Reset(view)) return Flat::LayoutResult::UnknownView;
        Notify(kReset, view);
        return Flat::LayoutResult::Ok;
    }

    Flat::LayoutResult Save() noexcept {
        EnsureLoaded();
        std::lock_guard fileLock{g_fileMutex};
        const auto path = LayoutFilePath();
        if (path.empty()) return Flat::LayoutResult::IoFailed;
        const std::string data = L::Instance().Serialize();
        if (data.size() > L::kMaxFileBytes) return Flat::LayoutResult::IoFailed;
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        auto temporary = path;
        temporary += L".tmp";
        {
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            if (!stream) return Flat::LayoutResult::IoFailed;
            stream.write(data.data(), static_cast<std::streamsize>(data.size()));
            stream.flush();
            if (!stream) return Flat::LayoutResult::IoFailed;
        }
        if (L::Instance().FutureVersionLoaded() || g_loadRejected.load(std::memory_order_acquire)) {
            auto backup = path;
            backup += L".bak";
            if (std::filesystem::exists(path, ec) && !std::filesystem::exists(backup, ec) &&
                !CopyFileW(path.c_str(), backup.c_str(), TRUE)) {
                std::filesystem::remove(temporary, ec);
                return Flat::LayoutResult::IoFailed;
            }
        }
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::filesystem::remove(temporary, ec);
            return Flat::LayoutResult::IoFailed;
        }
        g_loadRejected.store(false, std::memory_order_release);
        L::Instance().ClearDirty();
        return Flat::LayoutResult::Ok;
    }

    Flat::LayoutResult Load() noexcept {
        Flat::LayoutResult result;
        {
            std::lock_guard fileLock{g_fileMutex};
            result = LoadFromDisk();
            g_loaded.store(true, std::memory_order_release);
        }
        if (result == Flat::LayoutResult::Ok) Notify(kLoaded, 0);
        return result;
    }

    Flat::LayoutResult SetEditMode(bool enabled) noexcept {
        EnsureLoaded();
        if (enabled) {
            if (L::Instance().EditMode()) return Flat::LayoutResult::Ok;
            if (!WebRuntime::LayoutEditorSupported()) return Flat::LayoutResult::Unavailable;
            if (const auto focused = WebRuntime::GetFocusedView()) WebRuntime::Unfocus(focused);
            if (!WebRuntime::ShowLayoutEditor()) return Flat::LayoutResult::Unavailable;
            const auto editor = WebRuntime::LayoutEditorView();
            WebRuntime::RegisterJSListener(editor, "__prismaLayoutNative", HandleEditorMessage);
            L::Instance().SetEditMode(true);
            PushEditorState();
            NotifyEditMode(true);
        } else {
            if (!L::Instance().EditMode()) return Flat::LayoutResult::Ok;
            L::Instance().SetEditMode(false);
            WebRuntime::HideLayoutEditor();
            if (L::Instance().Dirty()) (void)Save();
            NotifyEditMode(false);
        }
        Notify(kEditMode, 0, false);
        return Flat::LayoutResult::Ok;
    }

    bool IsEditMode() noexcept { return L::Instance().EditMode(); }

    std::uint32_t Subscribe(Flat::LayoutChangeCallback callback, void* userdata) noexcept {
        if (!callback) return 0;
        std::lock_guard lock{g_subscriberMutex};
        if (g_subscribers.size() >= kMaxSubscribers) return 0;
        const std::uint32_t token = g_nextToken++;
        g_subscribers.push_back({token, callback, userdata});
        return token;
    }

    Flat::LayoutResult Unsubscribe(std::uint32_t token) noexcept {
        {
            std::lock_guard lock{g_subscriberMutex};
            const auto it = std::find_if(g_subscribers.begin(), g_subscribers.end(),
                                         [token](const Subscriber& s) { return s.token == token; });
            if (it == g_subscribers.end()) return Flat::LayoutResult::InvalidArgument;
            g_subscribers.erase(it);
        }
        std::lock_guard delivery{g_deliveryMutex};
        return Flat::LayoutResult::Ok;
    }

    void HandleEditorMessage(const std::string& message) {
        const auto separator = message.find('\n');
        const std::string_view channel{message.data(), separator == std::string::npos ? message.size() : separator};
        const std::string_view payload =
            separator == std::string::npos ? std::string_view{} : std::string_view{message}.substr(separator + 1);
        auto& state = L::Instance();
        if (channel == "ready") {
            PushEditorState();
            return;
        }
        if (channel == "rect") {
            double v[5]{};
            if (!SplitNumbers(payload, v, 5)) return;
            std::uint64_t view = 0;
            L::Rect rect;
            if (!ToViewId(v[0], view) || !L::ToFloat(v[1], rect.x) || !L::ToFloat(v[2], rect.y) ||
                !L::ToFloat(v[3], rect.w) || !L::ToFloat(v[4], rect.h))
                return;
            if (state.SetRect(view, rect)) Notify(kRect, view, false);
            return;
        }
        if (channel == "hidden") {
            double v[2]{};
            if (!SplitNumbers(payload, v, 2)) return;
            std::uint64_t view = 0;
            if (!ToViewId(v[0], view)) return;
            if (state.SetHidden(view, v[1] != 0.0)) Notify(kVisibility, view, false);
            return;
        }
        if (channel == "reset") {
            double v[1]{};
            if (!SplitNumbers(payload, v, 1)) return;
            std::uint64_t view = 0;
            if (!ToViewId(v[0], view)) return;
            if (state.Reset(view)) {
                Notify(kReset, view, false);
                PushEditorState();
            }
            return;
        }
        if (channel == "resetAll") {
            state.ResetAll();
            Notify(kReset, 0, false);
            PushEditorState();
            return;
        }
        if (channel == "settings") {
            double v[2]{};
            if (!SplitNumbers(payload, v, 2) || !std::isfinite(v[0])) return;
            state.SetSettings({static_cast<int>(std::clamp(v[0], 0.0, 64.0)), v[1] != 0.0});
            return;
        }
        if (channel == "save") {
            (void)Save();
            return;
        }
        if (channel == "close") RequestEditMode(false);
    }
}
