#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "PrismaUI/ModelPreviewImageSourcePolicy.h"

namespace {

using namespace PrismaUI::ModelPreview::ImageSourcePolicy;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "modelpreview_imagesource_policy_tests: %s\n", message);
        std::abort();
    }
}

struct FakeBackend {
    std::string url;
    bool live = false;
    std::vector<std::string> calls;

    std::string ReserveExternalImage(std::uint64_t key) {
        calls.push_back("reserve:" + std::to_string(key));
        return url;
    }

    void PublishExternalImage(std::uint64_t key, int srv, std::uint32_t width, std::uint32_t height,
                              std::uint64_t generation) {
        calls.push_back("publish:" + std::to_string(key) + ":" + std::to_string(srv) + ":" +
                        std::to_string(width) + ":" + std::to_string(height) + ":" + std::to_string(generation));
    }

    bool IsExternalImageLive(std::uint64_t) const { return live; }
};

}

int main() {
    {
        FakeBackend backend{"/prisma-mp/0000000000000001.imgsrc", false, {}};
        State state;
        Check(!Reconcile(state, backend, 7, false, 1, 512, 512, 1), "inactive preview must not reconcile");
        Check(backend.calls.empty(), "inactive preview must not call the backend");
        Check(!ExcludesHoleOverlay(state), "inactive preview must keep the hole overlay");
    }
    {
        FakeBackend backend{"", false, {}};
        State state;
        Check(!Reconcile(state, backend, 7, true, 1, 512, 512, 1), "an empty reservation must not report reserved");
        Check(backend.calls == std::vector<std::string>{"reserve:7"}, "an empty reservation must not publish");
        Check(state.url.empty() && !state.published, "an empty reservation must leave no state");
        Check(!ExcludesHoleOverlay(state), "no URL must keep the hole overlay");
    }
    {
        FakeBackend backend{"/prisma-mp/0000000000000001.imgsrc", false, {}};
        State state;
        Check(Reconcile(state, backend, 7, true, 1, 512, 512, 1), "first reservation must report reserved");
        Check(backend.calls == std::vector<std::string>({"reserve:7", "publish:7:1:512:512:1"}),
              "first reservation must publish once");
        Check(!ExcludesHoleOverlay(state), "an issued publish must keep the hole overlay until the bind is live");
        backend.live = true;
        state.live = backend.IsExternalImageLive(7);
        Check(ExcludesHoleOverlay(state), "a live bind must exclude the hole overlay");
        Check(!Reconcile(state, backend, 7, true, 1, 512, 512, 1), "same generation must not report reserved");
        Check(backend.calls.size() == 2, "same generation must not publish again");
        Check(!Reconcile(state, backend, 7, true, 1, 512, 512, 2) && backend.calls.size() == 3,
              "a new generation must publish once");
        Check(backend.calls.back() == "publish:7:1:512:512:2", "a new generation must publish the new generation");
        backend.live = false;
        state.live = backend.IsExternalImageLive(7);
        Check(!ExcludesHoleOverlay(state), "a bind that stops being live must restore the hole overlay");
        backend.live = true;
        state.live = backend.IsExternalImageLive(7);
        Check(ExcludesHoleOverlay(state), "a bind that becomes live again must exclude the hole overlay");
        Check(Retire(state), "a published state must report a retract");
        Check(!state.published && !state.live && !ExcludesHoleOverlay(state),
              "a retired state must show the hole overlay");
        Check(!Retire(state), "a second retire must not report a retract");
        Check(!Reconcile(state, backend, 7, true, 2, 512, 512, 2) && backend.calls.size() == 4,
              "a pooled target must publish again at the same generation");
        Check(backend.calls.back() == "publish:7:2:512:512:2", "a pooled target must publish its own SRV");
    }
    {
        State state;
        state.published = true;
        state.live = true;
        Check(!ExcludesHoleOverlay(state), "a publish without a URL must not exclude the hole overlay");
    }
    {
        State state;
        state.url = "/prisma-mp/0000000000000001.imgsrc";
        state.live = true;
        Check(!ExcludesHoleOverlay(state), "a live bind without an issued publish must not exclude the hole overlay");
    }
    std::printf("modelpreview_imagesource_policy_tests passed\n");
    return 0;
}
