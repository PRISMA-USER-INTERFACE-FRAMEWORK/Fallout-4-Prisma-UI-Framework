#pragma once

#include "ModelPreview.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PrismaUI::TextureOverlay {
using ViewId = std::uint64_t;
inline constexpr int kApiVersion = 1;

void Show(ViewId viewId, const std::string& jsonArgs);
void Hide(ViewId viewId, const std::string& jsonArgs);

void GetOverlays(ViewId viewId, std::vector<ModelPreview::Overlay>& out);

void GetActiveViews(std::vector<ViewId>& out);

void DrainRemovedOverlayKeys(std::vector<uint64_t>& out);

bool HasPendingWork();

using StatusSink = void (*)(ViewId viewId, const char* json);
void SetStatusSink(StatusSink fn);

void OnViewDestroyed(ViewId viewId);
}
