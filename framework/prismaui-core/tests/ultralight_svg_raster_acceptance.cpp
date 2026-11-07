#include <AppCore/Platform.h>
#include <Ultralight/Ultralight.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

    std::string ToUtf8(const ultralight::String& value) {
        const auto& utf8 = value.utf8();
        return std::string(utf8.data(), utf8.size());
    }

    std::string ReadText(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return {};
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    class FixtureFileSystem final : public ultralight::FileSystem {
    public:
        FixtureFileSystem(const std::filesystem::path& root, const std::filesystem::path& runtimeRoot)
            : html_(ReadText(root / "svg_hairline_acceptance.html")),
              svg_(ReadText(root / "svg_hairline.svg")),
              delegate_(ultralight::GetPlatformFileSystem(runtimeRoot.string().c_str())) {}

        bool ready() const { return !html_.empty() && !svg_.empty() && delegate_ != nullptr; }

        bool FileExists(const ultralight::String& path) override {
            const auto value = ToUtf8(path);
            if (value.ends_with("raster.html") || value.ends_with("svg_hairline.svg")) return true;
            return delegate_ && delegate_->FileExists(path);
        }

        ultralight::String GetFileMimeType(const ultralight::String& path) override {
            const auto value = ToUtf8(path);
            if (value.ends_with("raster.html")) return ultralight::String("text/html");
            if (value.ends_with("svg_hairline.svg")) return ultralight::String("image/svg+xml");
            return delegate_ ? delegate_->GetFileMimeType(path) : ultralight::String();
        }

        ultralight::String GetFileCharset(const ultralight::String& path) override {
            const auto value = ToUtf8(path);
            if (value.ends_with("raster.html") || value.ends_with("svg_hairline.svg"))
                return ultralight::String("UTF-8");
            return delegate_ ? delegate_->GetFileCharset(path) : ultralight::String();
        }

        ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String& path) override {
            const auto value = ToUtf8(path);
            const std::string* data = nullptr;
            if (value.ends_with("raster.html")) data = &html_;
            if (value.ends_with("svg_hairline.svg")) data = &svg_;
            if (data) return ultralight::Buffer::CreateFromCopy(data->data(), data->size());
            return delegate_ ? delegate_->OpenFile(path) : nullptr;
        }

    private:
        std::string html_;
        std::string svg_;
        ultralight::FileSystem* delegate_ = nullptr;
    };

    class LoadState final : public ultralight::LoadListener {
    public:
        void OnDOMReady(ultralight::View*, uint64_t, bool mainFrame, const ultralight::String&) override {
            if (mainFrame) domReady = true;
        }

        void OnFailLoading(ultralight::View*, uint64_t, bool mainFrame, const ultralight::String&,
                           const ultralight::String&, const ultralight::String&, int) override {
            if (mainFrame) loadFailed = true;
        }

        bool domReady = false;
        bool loadFailed = false;
    };

    template <typename Renderer>
    void RefreshDisplayIfSupported(Renderer* renderer) {
        if constexpr (requires(Renderer& value) { value.RefreshDisplay(0); }) {
            renderer->RefreshDisplay(0);
        }
    }

    bool PumpUntil(ultralight::Renderer* renderer, const std::function<bool()>& predicate, const int ticks) {
        for (int tick = 0; tick != ticks; ++tick) {
            renderer->Update();
            RefreshDisplayIfSupported(renderer);
            renderer->Render();
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return predicate();
    }

    struct RasterCase {
        std::string kind;
        int size = 0;
        bool supported = false;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

    struct Measurement {
        RasterCase rasterCase;
        std::uint64_t litPixels = 0;
        double energy = 0.0;
    };

    std::vector<std::string> Split(const std::string& value, const char separator) {
        std::vector<std::string> parts;
        std::size_t start = 0;
        while (start <= value.size()) {
            const auto end = value.find(separator, start);
            parts.push_back(value.substr(start, end == std::string::npos ? std::string::npos : end - start));
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return parts;
    }

    std::vector<RasterCase> ParseCases(const std::string& encoded) {
        std::vector<RasterCase> cases;
        for (const auto& entry : Split(encoded, '|')) {
            const auto fields = Split(entry, ',');
            if (fields.size() != 7) continue;
            RasterCase value;
            value.kind = fields[0];
            value.size = std::stoi(fields[1]);
            value.supported = fields[2] == "1";
            value.x = std::stoi(fields[3]);
            value.y = std::stoi(fields[4]);
            value.width = std::stoi(fields[5]);
            value.height = std::stoi(fields[6]);
            cases.push_back(std::move(value));
        }
        return cases;
    }

    bool HasExpectedCaseSet(const std::vector<RasterCase>& cases) {
        if (cases.size() != 16) return false;
        for (const auto& value : cases) {
            if (value.width != value.size || value.height != value.size) return false;
        }
        for (const std::string kind : {"img", "mask", "webkit-mask", "inline"}) {
            for (const int size : {13, 16, 20, 24}) {
                const auto matches = std::count_if(cases.begin(), cases.end(), [&](const RasterCase& value) {
                    return value.kind == kind && value.size == size;
                });
                if (matches != 1) return false;
            }
        }
        return true;
    }

    Measurement Measure(const RasterCase& rasterCase, const ultralight::Surface& surface,
                        const std::uint8_t* pixels) {
        Measurement measurement;
        measurement.rasterCase = rasterCase;
        if (!rasterCase.supported) return measurement;
        const int left = std::clamp(rasterCase.x, 0, static_cast<int>(surface.width()));
        const int top = std::clamp(rasterCase.y, 0, static_cast<int>(surface.height()));
        const int right = std::clamp(rasterCase.x + rasterCase.width, 0, static_cast<int>(surface.width()));
        const int bottom = std::clamp(rasterCase.y + rasterCase.height, 0, static_cast<int>(surface.height()));
        for (int y = top; y < bottom; ++y) {
            const auto* row = pixels + static_cast<std::size_t>(y) * surface.row_bytes();
            for (int x = left; x < right; ++x) {
                const auto* pixel = row + static_cast<std::size_t>(x) * 4;
                const int brightness = std::max({static_cast<int>(pixel[0]), static_cast<int>(pixel[1]),
                                                 static_cast<int>(pixel[2])});
                const int signal = std::max(0, brightness - 16);
                if (signal >= 8) ++measurement.litPixels;
                measurement.energy += static_cast<double>(signal) / 255.0;
            }
        }
        return measurement;
    }

    const Measurement* Find(const std::vector<Measurement>& measurements, const std::string& kind, const int size) {
        for (const auto& measurement : measurements) {
            if (measurement.rasterCase.kind == kind && measurement.rasterCase.size == size) return &measurement;
        }
        return nullptr;
    }

}

int main(int argc, char** argv) {
    const std::filesystem::path fixtureRoot = argc > 1
        ? std::filesystem::path(argv[1])
        : std::filesystem::path("framework/prismaui-core/tests/svg_hairline");
    const char* sdkEnvironment = std::getenv("ULTRALIGHT_SDK_DIR");
    const std::filesystem::path sdkRoot = sdkEnvironment && *sdkEnvironment
        ? std::filesystem::path(sdkEnvironment)
        : std::filesystem::path("framework/prismaui-core/external/ultralight-1.4.0");
    const char* runtimeEnvironment = std::getenv("ULTRALIGHT_RUNTIME_DIR");
    const std::filesystem::path runtimeRoot = runtimeEnvironment && *runtimeEnvironment
        ? std::filesystem::path(runtimeEnvironment)
        : sdkRoot;
    if (!std::filesystem::is_directory(runtimeRoot / "resources")) {
        std::cerr << "Ultralight runtime resources not found under " << runtimeRoot.string() << "\n";
        return 1;
    }

    FixtureFileSystem fileSystem(fixtureRoot, runtimeRoot);
    if (!fileSystem.ready()) {
        std::cerr << "SVG raster fixture files or runtime filesystem could not be initialized\n";
        return 1;
    }

    ultralight::Config config;
    config.cache_path = "";
    auto& platform = ultralight::Platform::instance();
    platform.set_config(config);
    platform.set_file_system(&fileSystem);
    platform.set_font_loader(ultralight::GetPlatformFontLoader());
    platform.set_gpu_driver(nullptr);

    auto renderer = ultralight::Renderer::Create();
    if (!renderer) {
        std::cerr << "Ultralight Renderer::Create failed\n";
        return 1;
    }
    auto session = renderer->CreateSession(false, "svg-raster-acceptance");
    if (!session) {
        std::cerr << "Ultralight Session creation failed\n";
        return 1;
    }

    ultralight::ViewConfig viewConfig;
    viewConfig.is_accelerated = false;
    viewConfig.initial_device_scale = 1.0;
    LoadState loadState;
    auto view = renderer->CreateView(640, 360, viewConfig, session);
    if (!view) {
        std::cerr << "Ultralight View creation failed\n";
        return 1;
    }

    view->set_load_listener(&loadState);
    view->LoadURL("file:///raster.html");
    if (!PumpUntil(renderer.get(), [&] { return loadState.domReady || loadState.loadFailed; }, 400) ||
        loadState.loadFailed) {
        std::cerr << "SVG raster fixture did not reach DOMReady\n";
        return 1;
    }

    std::string encodedCases;
    if (!PumpUntil(renderer.get(), [&] {
        ultralight::String exception;
        const auto value = view->EvaluateScript(
            "window.__prisma_raster_ready ? String(window.__prisma_raster_cases || '') : ''", &exception);
        encodedCases = ToUtf8(value);
        return exception.empty() && !encodedCases.empty();
    }, 300)) {
        std::cerr << "SVG raster fixture did not publish measurement rectangles\n";
        return 1;
    }

    for (int tick = 0; tick != 20; ++tick) {
        renderer->Update();
        RefreshDisplayIfSupported(renderer.get());
        renderer->Render();
    }

    const auto cases = ParseCases(encodedCases);
    if (!HasExpectedCaseSet(cases)) {
        std::cerr << "SVG raster fixture did not publish the exact four-path by four-size case matrix\n";
        return 1;
    }

    auto* surface = view->surface();
    if (!surface) {
        std::cerr << "CPU SVG raster acceptance has no Surface\n";
        return 1;
    }
    const auto* pixels = static_cast<const std::uint8_t*>(surface->LockPixels());
    if (!pixels) {
        std::cerr << "CPU SVG raster acceptance could not lock Surface pixels\n";
        return 1;
    }

    std::vector<Measurement> measurements;
    measurements.reserve(cases.size());
    for (const auto& rasterCase : cases) measurements.push_back(Measure(rasterCase, *surface, pixels));
    surface->UnlockPixels();
    view->set_load_listener(nullptr);

    const char* label = std::getenv("PRISMA_ULTRALIGHT_RUNTIME_LABEL");
    std::cout << "raster_runtime label=" << (label ? label : "unknown") << " device_scale=1.0\n";
    std::cout << std::fixed << std::setprecision(3);
    for (const auto& measurement : measurements) {
        std::cout << "raster_case kind=" << measurement.rasterCase.kind
                  << " size=" << measurement.rasterCase.size
                  << " supported=" << (measurement.rasterCase.supported ? 1 : 0)
                  << " lit_pixels=" << measurement.litPixels
                  << " energy=" << measurement.energy << "\n";
    }

    for (const std::string kind : {"img", "inline"}) {
        const auto* reference = Find(measurements, kind, 24);
        if (!reference || reference->litPixels == 0 || reference->energy <= 1.0) {
            std::cerr << "SVG raster fixture reference case failed for " << kind << " at 24px\n";
            return 1;
        }
    }

    if (const char* require = std::getenv("PRISMA_SVG_REQUIRE_13PX_VISIBLE"); require && std::string(require) == "1") {
        for (const auto& measurement : measurements) {
            if (measurement.rasterCase.size != 13 || !measurement.rasterCase.supported) continue;
            if (measurement.litPixels == 0) {
                std::cerr << "SVG raster acceptance failed: " << measurement.rasterCase.kind
                          << " disappeared at 13px\n";
                return 1;
            }
        }
    }

    std::cout << "svg raster measurement complete\n";
    return 0;
}
