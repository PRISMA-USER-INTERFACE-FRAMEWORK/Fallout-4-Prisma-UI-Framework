#include "PCH.h"

#include "PrismaUI_F4VR_API.h"

#include "PrismaUI/WebRuntime.h"
#include "VR/SpatialPointer.h"
#include "VR/SpatialPresentation.h"
#include "VR/VRNetworkPolicy.h"
#include "VR/VRViewState.h"

namespace PrismaUI::VRApi
{
    namespace
    {
        using namespace PRISMA_UI_VR_API;

        class Interface final : public IVPrismaUIVR1
        {
        public:
            [[nodiscard]] static Interface* GetSingleton() noexcept
            {
                static Interface singleton;
                return &singleton;
            }

            SpatialResult GetSpatialCapabilities(
                SpatialCapabilitiesV1* outCapabilities) noexcept override
            {
                return SpatialPresentation::GetCapabilities(outCapabilities);
            }

            SpatialResult SubmitSpatialUpdate(
                PrismaView view,
                const SpatialUpdateV1* update) noexcept override
            {
                if (!VR::AcquireView(view)) {
                    return SpatialResult::InvalidView;
                }
                return SpatialPresentation::SubmitUpdate(view, update);
            }

            SpatialResult GetSpatialState(
                PrismaView view,
                SpatialStateV1* outState) noexcept override
            {
                return SpatialPresentation::GetState(view, outState);
            }

            PrismaView CreateViewWithOptions(
                const char* htmlPath,
                PRISMA_UI_API::OnDomReadyCallback onDomReadyCallback,
                const ViewCreateOptionsV1* options) noexcept override
            {
                if (!htmlPath) {
                    return 0;
                }
                if (options && options->structSize < sizeof(ViewCreateOptionsV1)) {
                    return 0;
                }

                const auto policy = options ?
                    options->networkAccessPolicy :
                    NetworkAccessPolicy::RemoteNoFile;

                const auto view = WebRuntime::CreateView(
                    htmlPath,
                    [onDomReadyCallback](WebRuntime::ViewId ready) {
                        VRNetworkPolicy::ReapplyOnDomReady(ready);
                        if (onDomReadyCallback) {
                            onDomReadyCallback(ready);
                        }
                    },
                    nullptr);
                if (view == 0) {
                    return 0;
                }
                (void)VRNetworkPolicy::Set(view, policy);
                return view;
            }

            bool SetNetworkAccessPolicy(
                PrismaView view,
                NetworkAccessPolicy policy) noexcept override
            {
                return VRNetworkPolicy::Set(view, policy);
            }

            bool GetNetworkAccessPolicy(
                PrismaView view,
                NetworkAccessPolicy* outPolicy) noexcept override
            {
                return VRNetworkPolicy::Get(view, outPolicy);
            }

            SpatialResult SubmitSpatialPointerUpdate(
                PrismaView view,
                const SpatialPointerUpdateV1* update) noexcept override
            {
                if (!VR::AcquireView(view)) {
                    return SpatialResult::InvalidView;
                }
                return SpatialPointer::SubmitUpdate(view, update);
            }

            SpatialResult CancelSpatialPointer(PrismaView view) noexcept override
            {
                return SpatialPointer::Cancel(view);
            }

            SpatialResult GetSpatialPointerState(
                PrismaView view,
                SpatialPointerStateV1* outState) noexcept override
            {
                return SpatialPointer::GetState(view, outState);
            }
        };
    }

    [[nodiscard]] void* Request(InterfaceVersion version) noexcept
    {
        switch (version) {
        case InterfaceVersion::V1:
            return static_cast<IVPrismaUIVR1*>(Interface::GetSingleton());
        default:
            return nullptr;
        }
    }
}

extern "C" __declspec(dllexport) void* F4SEAPI RequestPluginVRAPI(
    const PRISMA_UI_VR_API::InterfaceVersion a_interfaceVersion)
{
    return PrismaUI::VRApi::Request(a_interfaceVersion);
}
