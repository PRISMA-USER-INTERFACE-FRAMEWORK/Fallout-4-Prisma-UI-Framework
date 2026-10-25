# Fallout 4 VR extension (`IVPrismaUIVR1`)

> **Status: released with PrismaUI_F4 2.2.0.**
>
> `PrismaUI_F4VR.dll` is a separate provider built against CommonLibF4VR. The `framework-v2.2.0` release publishes a dedicated `PrismaUI_F4-2.2.0-Fallout4-VR.zip` package. Treat optional spatial capabilities as capability-gated even when the VR provider is installed.

## Provider model

Flat Fallout 4 and Fallout 4 VR use different provider DLLs:

```text
Fallout 4 desktop -> PrismaUI_F4.dll
Fallout 4 VR      -> PrismaUI_F4VR.dll
```

The shared header knows both provider names. The VR extension is additive and does not alter the desktop V1-V12 vtable layout.

The current framework architecture is Ultralight-based. Do not apply the retired legacy-runtime Host/subprocess model to the current VR provider.

## Requesting the APIs

```cpp
#include "PrismaUI_F4_API.h"
#include "PrismaUI_F4VR_API.h"

auto* ui = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI10>();
auto* vr = PRISMA_UI_VR_API::RequestPluginVRAPI<PRISMA_UI_VR_API::IVPrismaUIVR1>();

if (!vr) {
    // Not running against a provider that exposes the VR extension.
}
```

Treat a non-null VR interface as API availability, not proof that every optional spatial feature is available. Query capabilities.

## Presentation modes

| Mode | Behavior |
|---|---|
| `HeadLockedQuad` | Fixed relative to the headset |
| `WorldBillboard` | World-positioned and faces the viewer |
| `WorldQuad` | World-positioned and uses the supplied orientation |

Pixel dimensions control page resolution. Physical dimensions control world-space size.

## Submit a spatial placement

```cpp
PRISMA_UI_VR_API::SpatialUpdateV1 update{};
update.structSize       = sizeof(update);
update.coordinateSpace  = PRISMA_UI_VR_API::SpatialCoordinateSpace::GameWorld;
update.presentationMode = PRISMA_UI_VR_API::SpatialPresentationMode::WorldBillboard;
update.sequence         = ++mySequence;
update.pose.position[0] = x;
update.pose.position[1] = y;
update.pose.position[2] = z;
update.pose.orientation[3] = 1.0f;
update.dimensions.pixelWidth     = 1024;
update.dimensions.pixelHeight    = 768;
update.dimensions.physicalWidth  = 60.0f;
update.dimensions.physicalHeight = 45.0f;

const auto result = vr->SubmitSpatialUpdate(view, &update);
```

Updates are latest-only. If a newer update replaces a pending one, `PendingUpdateReplaced` is a successful coalescing result, not a hard failure.

Use state readback when you need to distinguish accepted from applied sequence numbers.

## Pointer input

World-space pointer input is submitted as a ray and translated into page input when the provider can resolve a hit.

```cpp
PRISMA_UI_VR_API::SpatialPointerUpdateV1 pointer{};
pointer.structSize   = sizeof(pointer);
pointer.flags        = PRISMA_UI_VR_API::SpatialPointerUpdate_Active;
pointer.buttonLevels = trigger
    ? PRISMA_UI_VR_API::SpatialPointerButton_Primary
    : 0;
pointer.sequence     = ++myPointerSequence;
pointer.maxDistance  = 400.0f;
pointer.pointerSourceId =
    PRISMA_UI_VR_API::SpatialPointerSource_PhysicalRightController;

vr->SubmitSpatialPointerUpdate(view, &pointer);
```

Call `CancelSpatialPointer` when the source goes away or a view is hidden during interaction so held/hover state is not left behind.

## Read back pointer state

```cpp
PRISMA_UI_VR_API::SpatialPointerStateV1 state{};
state.structSize = sizeof(state);

if (vr->GetSpatialPointerState(view, &state) ==
    PRISMA_UI_VR_API::SpatialResult::Success) {
    const bool hitting = state.hitDistance >= 0.0f;
}
```

Useful fields include accepted/applied sequence numbers, hit pixel/UV coordinates, held button levels, replaced-pending count, and the most recent apply result.

## Capabilities

Query `GetSpatialCapabilities` instead of assuming optional features:

- supported presentation modes;
- maximum pixel dimensions;
- spatial view/resource limits;
- feature bits such as scene-depth occlusion;
- native network-policy capability where implemented.

An API shape existing in the header does not make an optional feature available on every provider/runtime.

## Network policy

VR network-policy methods use the same framework-wide safety model as the current Ultralight architecture.

Important semantics:

- `file://` is not a supported Prisma content path;
- local Prisma/plugin assets use the confined Prisma content path;
- private/loopback targets remain subject to native safety policy and explicit narrow exceptions;
- redirects are checked through the same policy boundary;
- `LocalOnly` adds a strict no-remote restriction;
- `Unrestricted` means the normal Prisma-approved network path, **not** “disable all Prisma security”;
- `RemoteNoFile` is retained for ABI compatibility and does not enable arbitrary local filesystem URLs.

Do not repeat the old legacy-runtime note that `LocalOnly` is merely a one-way injected page script. The current policy contract is documented in the Matrix network-policy semantics and is intentionally backend-safe rather than legacy-runtime-specific.

VR network-policy behavior remains capability-gated and should be verified on the runtime families a mod claims to support.

## Results

Spatial methods return `SpatialResult`. Treat success/replacement, unsupported, not-ready, and validation failures according to the method contract rather than assuming a failed frame means the provider is absent.

## Release boundary

PrismaUI_F4 2.2.0 publishes desktop and VR packages separately. The VR package establishes release availability of `PrismaUI_F4VR.dll`; it does not imply that every desktop feature exists in VR or that every optional spatial capability is available on every setup.

Keep VR-only dependencies conditional on the VR provider and query the specific capability before using it.
