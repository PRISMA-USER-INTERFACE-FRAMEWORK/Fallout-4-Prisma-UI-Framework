#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
struct tagRECT;
typedef struct tagRECT RECT;

namespace PrismaUI::ModelPreview {

using ViewId = uint64_t;

inline constexpr int kApiVersion = 4;

struct Overlay {
    ID3D11ShaderResourceView* srv = nullptr;

    uint32_t sourceWidth = 0, sourceHeight = 0;
    long destLeft = 0, destTop = 0, destRight = 0, destBottom = 0;
    bool hasClip = false;
    long clipLeft = 0, clipTop = 0, clipRight = 0, clipBottom = 0;
    uint64_t contentGeneration = 0;
    uint64_t continuityKey = 0;
};

bool Enabled();

void Show(ViewId viewId, const std::string& jsonArgs);
void Hide(ViewId viewId, const std::string& jsonArgs);

void TickCore(ID3D11Device* dev, ID3D11DeviceContext* ctx);

void GetOverlays(ViewId viewId, std::vector<Overlay>& out);

void GetActiveViews(std::vector<ViewId>& out);

void DrainRemovedOverlayKeys(std::vector<uint64_t>& out);

bool HasPendingWork();

using ViewGateFn = bool (*)(ViewId);
void SetViewGate(ViewGateFn fn);
void SetGameLoadActive(bool active) noexcept;

using StatusSink = void (*)(ViewId viewId, const char* json);
void SetStatusSink(StatusSink fn);

void OnViewDestroyed(ViewId viewId);

void RequestMemorySample(const char* a_reason);

void NoteViewCreated();

void Shutdown();

}
