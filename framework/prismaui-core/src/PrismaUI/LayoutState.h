#pragma once

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace PrismaUI::Layout {

    inline constexpr float kStageWidth = 1920.0f;
    inline constexpr float kStageHeight = 1080.0f;
    inline constexpr float kMinSize = 32.0f;
    inline constexpr std::size_t kMaxFileBytes = 256 * 1024;
    inline constexpr std::size_t kMaxSavedViews = 256;
    inline constexpr std::size_t kMaxKeyBytes = 192;
    inline constexpr std::size_t kMaxTextBytes = 64;
    inline constexpr std::size_t kMaxTitleBytes = 96;
    inline constexpr std::uint32_t kFileVersion = 1;

    struct Rect {
        float x = 0.0f;
        float y = 0.0f;
        float w = kStageWidth;
        float h = kStageHeight;
        bool operator==(const Rect& other) const noexcept {
            return x == other.x && y == other.y && w == other.w && h == other.h;
        }
    };

    struct Dest {
        float left = 0.0f;
        float top = 0.0f;
        float right = 1.0f;
        float bottom = 1.0f;
    };

    struct Settings {
        int snap = 10;
        bool grid = false;
    };

    struct Entry {
        std::string key;
        std::string owner;
        std::string title;
        Rect rect;
        Rect defaultRect;
        bool hidden = false;
        bool registered = false;
    };

    inline std::string TruncateUtf8(std::string_view value, std::size_t maxBytes) {
        if (value.size() <= maxBytes) return std::string{value};
        std::size_t cut = maxBytes;
        while (cut > 0 && (static_cast<unsigned char>(value[cut]) & 0xC0) == 0x80) --cut;
        return std::string{value.substr(0, cut)};
    }

    inline bool ToFloat(double value, float& out) noexcept {
        if (!std::isfinite(value) || std::fabs(value) > 1.0e6) return false;
        out = static_cast<float>(value);
        return true;
    }

    inline bool IsFinite(const Rect& r) noexcept {
        return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.w) && std::isfinite(r.h);
    }

    inline Rect Clamp(Rect r) noexcept {
        r.w = std::clamp(r.w, kMinSize, kStageWidth * 2.0f);
        r.h = std::clamp(r.h, kMinSize, kStageHeight * 2.0f);
        r.x = std::clamp(r.x, kMinSize - r.w, kStageWidth - kMinSize);
        r.y = std::clamp(r.y, kMinSize - r.h, kStageHeight - kMinSize);
        return r;
    }

    inline bool IsFullStage(const Rect& r) noexcept { return r == Rect{}; }

    inline Dest ToDest(const Rect& r) noexcept {
        return {r.x / kStageWidth, r.y / kStageHeight, (r.x + r.w) / kStageWidth, (r.y + r.h) / kStageHeight};
    }

    inline bool MapPoint(const Rect& r, int px, int py, int width, int height, int& outX, int& outY) noexcept {
        if (width <= 0 || height <= 0 || r.w <= 0.0f || r.h <= 0.0f) return false;
        const double stageX = static_cast<double>(px) * kStageWidth / width;
        const double stageY = static_cast<double>(py) * kStageHeight / height;
        outX = static_cast<int>(std::lround((stageX - r.x) / r.w * width));
        outY = static_cast<int>(std::lround((stageY - r.y) / r.h * height));
        return true;
    }

    inline std::string NormalizeKey(std::string_view path) {
        std::string key;
        key.reserve(path.size());
        for (const char c : path) {
            const char folded = c == '\\' ? '/' : (c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
            if (folded == '/' && !key.empty() && key.back() == '/') continue;
            key += folded;
        }
        while (!key.empty() && key.front() == '/') key.erase(key.begin());
        return key;
    }

    struct Json {
        enum class Kind { Null, Bool, Number, String, Array, Object } kind = Kind::Null;
        double number = 0.0;
        bool boolean = false;
        std::string text;
        std::vector<std::string> keys;
        std::vector<Json> values;
        const Json* Find(std::string_view name) const noexcept {
            if (kind != Kind::Object) return nullptr;
            for (std::size_t i = 0; i < keys.size(); ++i)
                if (keys[i] == name) return &values[i];
            return nullptr;
        }
    };

    class JsonReader {
    public:
        explicit JsonReader(std::string_view text) : m_text(text) {}
        bool Parse(Json& out) {
            if (!Value(out, 0)) return false;
            Space();
            return m_pos == m_text.size();
        }

    private:
        static constexpr int kMaxDepth = 8;
        std::string_view m_text;
        std::size_t m_pos = 0;

        void Space() {
            while (m_pos < m_text.size() && (m_text[m_pos] == ' ' || m_text[m_pos] == '\t' || m_text[m_pos] == '\n' ||
                                             m_text[m_pos] == '\r'))
                ++m_pos;
        }
        bool Take(char c) {
            Space();
            if (m_pos < m_text.size() && m_text[m_pos] == c) {
                ++m_pos;
                return true;
            }
            return false;
        }
        bool Literal(std::string_view word) {
            if (m_text.substr(m_pos, word.size()) != word) return false;
            m_pos += word.size();
            return true;
        }
        bool String(std::string& out) {
            if (!Take('"')) return false;
            out.clear();
            while (m_pos < m_text.size()) {
                const char c = m_text[m_pos++];
                if (c == '"') return true;
                if (static_cast<unsigned char>(c) < 0x20) return false;
                if (c != '\\') {
                    out += c;
                    continue;
                }
                if (m_pos >= m_text.size()) return false;
                const char e = m_text[m_pos++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u':
                        if (m_pos + 4 > m_text.size()) return false;
                        m_pos += 4;
                        out += '?';
                        break;
                    default: return false;
                }
            }
            return false;
        }
        bool Value(Json& out, int depth) {
            if (depth > kMaxDepth) return false;
            Space();
            if (m_pos >= m_text.size()) return false;
            const char c = m_text[m_pos];
            if (c == '{') {
                ++m_pos;
                out.kind = Json::Kind::Object;
                if (Take('}')) return true;
                for (;;) {
                    std::string key;
                    if (!String(key) || !Take(':')) return false;
                    Json child;
                    if (!Value(child, depth + 1)) return false;
                    out.keys.push_back(std::move(key));
                    out.values.push_back(std::move(child));
                    if (Take(',')) continue;
                    return Take('}');
                }
            }
            if (c == '[') {
                ++m_pos;
                out.kind = Json::Kind::Array;
                if (Take(']')) return true;
                for (;;) {
                    Json child;
                    if (!Value(child, depth + 1)) return false;
                    out.values.push_back(std::move(child));
                    if (Take(',')) continue;
                    return Take(']');
                }
            }
            if (c == '"') {
                out.kind = Json::Kind::String;
                return String(out.text);
            }
            if (c == 't' || c == 'f') {
                out.kind = Json::Kind::Bool;
                out.boolean = c == 't';
                return Literal(out.boolean ? "true" : "false");
            }
            if (c == 'n') return Literal("null");
            out.kind = Json::Kind::Number;
            const char* begin = m_text.data() + m_pos;
            const char* end = m_text.data() + m_text.size();
            const auto result = std::from_chars(begin, end, out.number);
            if (result.ec != std::errc{} || !std::isfinite(out.number)) return false;
            m_pos += static_cast<std::size_t>(result.ptr - begin);
            return true;
        }
    };

    inline void AppendEscaped(std::string& out, std::string_view value) {
        out += '"';
        for (const unsigned char c : value) {
            if (c == '"' || c == '\\') {
                out += '\\';
                out += static_cast<char>(c);
            } else if (c < 0x20) {
                out += ' ';
            } else {
                out += static_cast<char>(c);
            }
        }
        out += '"';
    }

    inline void AppendNumber(std::string& out, double value) {
        char buffer[48];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, 2);
        out.append(buffer, result.ptr);
    }

    class State {
    public:
        struct Saved {
            Rect rect;
            bool hasRect = false;
            bool hidden = false;
            std::string owner;
        };

        void Track(std::uint64_t view, const std::string& key, const std::string& owner, const std::string& title) {
            if (!view || key.empty()) return;
            std::lock_guard lock{m_mutex};
            Entry entry;
            entry.key = TruncateUtf8(key, kMaxKeyBytes);
            entry.owner = TruncateUtf8(owner, kMaxTextBytes);
            entry.title = title.empty() ? entry.key : TruncateUtf8(title, kMaxTitleBytes);
            ApplySaved(entry);
            m_entries[view] = std::move(entry);
            Bump();
        }

        void Untrack(std::uint64_t view) {
            std::lock_guard lock{m_mutex};
            if (m_entries.erase(view) != 0) Bump();
        }

        bool Register(std::uint64_t view, const std::string& title, const Rect* defaultRect) {
            std::lock_guard lock{m_mutex};
            const auto it = m_entries.find(view);
            if (it == m_entries.end()) return false;
            Entry& entry = it->second;
            if (!title.empty()) entry.title = TruncateUtf8(title, kMaxTitleBytes);
            if (defaultRect && IsFinite(*defaultRect)) {
                const bool followsDefault = entry.rect == entry.defaultRect;
                entry.defaultRect = Clamp(*defaultRect);
                if (followsDefault && !HasSavedRect(entry.key)) entry.rect = entry.defaultRect;
            }
            entry.registered = true;
            Bump();
            return true;
        }

        bool Unregister(std::uint64_t view) {
            std::lock_guard lock{m_mutex};
            const auto it = m_entries.find(view);
            if (it == m_entries.end() || !it->second.registered) return false;
            it->second.registered = false;
            Bump();
            return true;
        }

        bool Get(std::uint64_t view, Entry& out) const {
            std::lock_guard lock{m_mutex};
            const auto it = m_entries.find(view);
            if (it == m_entries.end()) return false;
            out = it->second;
            return true;
        }

        std::vector<std::pair<std::uint64_t, Entry>> List() const {
            std::lock_guard lock{m_mutex};
            return {m_entries.begin(), m_entries.end()};
        }

        bool SetRect(std::uint64_t view, const Rect& requested) {
            if (!IsFinite(requested)) return false;
            std::lock_guard lock{m_mutex};
            const auto it = m_entries.find(view);
            if (it == m_entries.end()) return false;
            const Rect clamped = Clamp(requested);
            if (it->second.rect == clamped) return true;
            it->second.rect = clamped;
            Persist(it->second);
            Bump();
            return true;
        }

        bool SetHidden(std::uint64_t view, bool hidden) {
            std::lock_guard lock{m_mutex};
            const auto it = m_entries.find(view);
            if (it == m_entries.end()) return false;
            if (it->second.hidden == hidden) return true;
            it->second.hidden = hidden;
            Persist(it->second);
            Bump();
            return true;
        }

        bool Reset(std::uint64_t view) {
            std::lock_guard lock{m_mutex};
            const auto it = m_entries.find(view);
            if (it == m_entries.end()) return false;
            it->second.rect = it->second.defaultRect;
            it->second.hidden = false;
            Persist(it->second);
            Bump();
            return true;
        }

        void ResetAll() {
            std::lock_guard lock{m_mutex};
            for (auto& [view, entry] : m_entries) {
                (void)view;
                entry.rect = entry.defaultRect;
                entry.hidden = false;
            }
            m_saved.clear();
            m_dirty = true;
            Bump();
        }

        bool UserHidden(std::uint64_t view) const {
            if (m_hiddenCount.load(std::memory_order_acquire) == 0) return false;
            std::lock_guard lock{m_mutex};
            const auto it = m_entries.find(view);
            return it != m_entries.end() && it->second.hidden;
        }

        bool Placement(std::uint64_t view, Rect& out) const {
            std::lock_guard lock{m_mutex};
            const auto it = m_entries.find(view);
            if (it == m_entries.end() || IsFullStage(it->second.rect)) return false;
            out = it->second.rect;
            return true;
        }

        bool MapInput(std::uint64_t view, int width, int height, int& x, int& y) const {
            Rect rect;
            if (!Placement(view, rect)) return false;
            return MapPoint(rect, x, y, width, height, x, y);
        }

        std::uint64_t Generation() const noexcept { return m_generation.load(std::memory_order_acquire); }

        bool EditMode() const noexcept { return m_editMode.load(std::memory_order_acquire); }
        bool SetEditMode(bool on) noexcept {
            const bool previous = m_editMode.exchange(on, std::memory_order_acq_rel);
            if (previous != on) Bump();
            return previous != on;
        }

        Settings GetSettings() const {
            std::lock_guard lock{m_mutex};
            return m_settings;
        }
        void SetSettings(const Settings& settings) {
            std::lock_guard lock{m_mutex};
            m_settings.snap = std::clamp(settings.snap, 0, 64);
            m_settings.grid = settings.grid;
            m_dirty = true;
        }

        bool FutureVersionLoaded() const {
            std::lock_guard lock{m_mutex};
            return m_future;
        }

        bool Dirty() const {
            std::lock_guard lock{m_mutex};
            return m_dirty;
        }
        void ClearDirty() {
            std::lock_guard lock{m_mutex};
            m_dirty = false;
        }

        std::string Serialize() const {
            std::lock_guard lock{m_mutex};
            std::string out = "{\"version\":" + std::to_string(kFileVersion) + ",\"editor\":{\"snap\":" +
                              std::to_string(m_settings.snap) + ",\"grid\":" + (m_settings.grid ? "true" : "false") +
                              "},\"views\":{";
            bool first = true;
            for (const auto& [key, saved] : m_saved) {
                if (!first) out += ',';
                first = false;
                AppendEscaped(out, key);
                out += ":{";
                if (saved.hasRect) {
                    out += "\"x\":";
                    AppendNumber(out, saved.rect.x);
                    out += ",\"y\":";
                    AppendNumber(out, saved.rect.y);
                    out += ",\"w\":";
                    AppendNumber(out, saved.rect.w);
                    out += ",\"h\":";
                    AppendNumber(out, saved.rect.h);
                    out += ',';
                }
                out += std::string{"\"hidden\":"} + (saved.hidden ? "true" : "false") + ",\"owner\":";
                AppendEscaped(out, saved.owner);
                out += '}';
            }
            out += "}}";
            return out;
        }

        bool Load(std::string_view text) {
            if (text.size() > kMaxFileBytes) return false;
            Json root;
            if (!JsonReader{text}.Parse(root) || root.kind != Json::Kind::Object) return false;
            const Json* version = root.Find("version");
            if (!version || version->kind != Json::Kind::Number || version->number < 1.0) return false;
            std::map<std::string, Saved> loaded;
            if (const Json* views = root.Find("views"); views && views->kind == Json::Kind::Object) {
                for (std::size_t i = 0; i < views->keys.size() && loaded.size() < kMaxSavedViews; ++i) {
                    const Json& item = views->values[i];
                    if (item.kind != Json::Kind::Object) continue;
                    const std::string key = NormalizeKey(views->keys[i]);
                    if (key.empty()) continue;
                    Saved saved;
                    const Json* x = item.Find("x");
                    const Json* y = item.Find("y");
                    const Json* w = item.Find("w");
                    const Json* h = item.Find("h");
                    Rect parsed;
                    if (x && y && w && h && x->kind == Json::Kind::Number && y->kind == Json::Kind::Number &&
                        w->kind == Json::Kind::Number && h->kind == Json::Kind::Number &&
                        ToFloat(x->number, parsed.x) && ToFloat(y->number, parsed.y) &&
                        ToFloat(w->number, parsed.w) && ToFloat(h->number, parsed.h)) {
                        saved.rect = Clamp(parsed);
                        saved.hasRect = true;
                    }
                    if (const Json* hidden = item.Find("hidden"); hidden && hidden->kind == Json::Kind::Bool)
                        saved.hidden = hidden->boolean;
                    if (const Json* owner = item.Find("owner"); owner && owner->kind == Json::Kind::String)
                        saved.owner = TruncateUtf8(owner->text, kMaxTextBytes);
                    if (saved.hasRect || saved.hidden) loaded[key] = std::move(saved);
                }
            }
            Settings settings;
            if (const Json* editor = root.Find("editor"); editor && editor->kind == Json::Kind::Object) {
                if (const Json* snap = editor->Find("snap"); snap && snap->kind == Json::Kind::Number)
                    settings.snap = static_cast<int>(std::clamp(snap->number, 0.0, 64.0));
                if (const Json* grid = editor->Find("grid"); grid && grid->kind == Json::Kind::Bool)
                    settings.grid = grid->boolean;
            }
            std::lock_guard lock{m_mutex};
            m_saved = std::move(loaded);
            m_settings = settings;
            m_future = version->number > static_cast<double>(kFileVersion);
            for (auto& [view, entry] : m_entries) {
                (void)view;
                entry.rect = entry.defaultRect;
                entry.hidden = false;
                ApplySaved(entry);
            }
            m_dirty = false;
            Bump();
            return true;
        }

    private:
        mutable std::mutex m_mutex;
        std::map<std::uint64_t, Entry> m_entries;
        std::map<std::string, Saved> m_saved;
        Settings m_settings;
        bool m_dirty = false;
        bool m_future = false;
        std::atomic<std::uint64_t> m_generation{1};
        std::atomic<std::uint32_t> m_hiddenCount{0};
        std::atomic<bool> m_editMode{false};

        void Bump() {
            std::uint32_t hidden = 0;
            for (const auto& [view, entry] : m_entries) {
                (void)view;
                if (entry.hidden) ++hidden;
            }
            m_hiddenCount.store(hidden, std::memory_order_release);
            m_generation.fetch_add(1, std::memory_order_acq_rel);
        }

        bool HasSavedRect(const std::string& key) const {
            const auto it = m_saved.find(key);
            return it != m_saved.end() && it->second.hasRect;
        }

        void ApplySaved(Entry& entry) const {
            const auto it = m_saved.find(entry.key);
            if (it == m_saved.end()) return;
            if (it->second.hasRect) entry.rect = it->second.rect;
            entry.hidden = it->second.hidden;
        }

        bool EvictDormant() {
            for (auto it = m_saved.begin(); it != m_saved.end(); ++it) {
                bool live = false;
                for (const auto& [view, entry] : m_entries) {
                    (void)view;
                    if (entry.key == it->first) live = true;
                }
                if (!live) {
                    m_saved.erase(it);
                    return true;
                }
            }
            return false;
        }

        void Persist(const Entry& entry) {
            m_dirty = true;
            const bool atDefault = entry.rect == entry.defaultRect;
            if (atDefault && !entry.hidden) {
                m_saved.erase(entry.key);
                return;
            }
            if (m_saved.size() >= kMaxSavedViews && m_saved.find(entry.key) == m_saved.end() && !EvictDormant()) return;
            Saved saved;
            saved.rect = entry.rect;
            saved.hasRect = !atDefault;
            saved.hidden = entry.hidden;
            saved.owner = entry.owner;
            m_saved[entry.key] = std::move(saved);
        }
    };

    inline State& Instance() {
        static State state;
        return state;
    }

}
