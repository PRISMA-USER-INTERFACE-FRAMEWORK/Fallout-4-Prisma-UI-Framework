#include "ModelPreviewInternal.h"

#include "IWebBackend.h"

namespace PrismaUI::ModelPreview {

void SyncImageSources(PrismaUI::Web::IWebBackend& backend) {
    for (const auto key : g_retractedImageKeys) backend.RetractExternalImage(key);
    g_retractedImageKeys.clear();
    for (const auto key : g_releasedImageKeys) backend.ReleaseExternalImage(key);
    g_releasedImageKeys.clear();
    const auto size = static_cast<uint32_t>(g_cfg.rtSize);
    for (auto& [fullKey, pv] : g_previews) {
        if (!pv.imageSource) continue;
        const bool hasTarget = pv.tex && pv.rtv && pv.srv && pv.model && pv.drawn;
        if (ImageSourcePolicy::Reconcile(pv.image, backend, pv.continuityKey, hasTarget, pv.srv, size, size,
                                         pv.lastRenderFrame + 1))
            EmitStatus(pv, true, "", "imagesource");
        if (!pv.image.url.empty()) pv.image.live = backend.IsExternalImageLive(pv.continuityKey);
    }
}

}
