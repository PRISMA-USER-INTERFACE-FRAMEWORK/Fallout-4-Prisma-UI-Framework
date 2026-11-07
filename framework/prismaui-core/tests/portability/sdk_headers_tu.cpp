// Portability guard for the shipped SDK headers.
//
// Headers must parse in two configurations:
//   * _WIN32 defined   -- what every consumer plugin actually builds as. Compiled against the
//                         stub in win32_stub/ so no Windows SDK is required.
//   * _WIN32 undefined -- so the portable half (types, enums, interface declarations) can be
//                         compiled by the Linux policy/VR test jobs without a Windows toolchain.
//
// The Win32-only provider-resolution helpers are referenced below so the guarded region is
// actually instantiated rather than merely skipped.

#include "PrismaUI_F4_Modern_API.h"
#include "PrismaUI_F4_API.h"
#include "PrismaUI_F4VR_API.h"

#include <type_traits>

// Portable in both configurations: these must never move inside the _WIN32 guard.
static_assert(sizeof(PrismaView) == 8, "PrismaView is the ABI view handle and must stay 64-bit");
static_assert(static_cast<int>(PRISMA_UI_API::InterfaceVersion::V1) == 0,
              "InterfaceVersion values are ABI; V1 must remain 0");
static_assert(static_cast<uint8_t>(PRISMA_UI_API::InterfaceVersion::V9) == 136u);
static_assert(static_cast<uint8_t>(PRISMA_UI_API::InterfaceVersion::V10) == 137u);
static_assert(static_cast<uint8_t>(PRISMA_UI_API::InterfaceVersion::V11) == 138u);
static_assert(static_cast<uint8_t>(PRISMA_UI_API::InterfaceVersion::V12) == 139u);
static_assert(std::is_base_of_v<PRISMA_UI_API::IVPrismaUI11, PRISMA_UI_API::IVPrismaUI12>);
static_assert(PRISMA_UI_FLAT_API::CoreApiVersion == 1);
static_assert(PRISMA_UI_FLAT_API::ControllerApiVersion == 1);
static_assert(PRISMA_UI_FLAT_API::GameThreadApiVersion == 1);
static_assert(static_cast<uint32_t>(PRISMA_UI_FLAT_API::ApiFeature::Core) == 1);
static_assert(static_cast<uint32_t>(PRISMA_UI_FLAT_API::ApiFeature::Controller) == 2);
static_assert(static_cast<uint32_t>(PRISMA_UI_FLAT_API::ApiFeature::GameThread) == 3);
static_assert(static_cast<uint32_t>(PRISMA_UI_FLAT_API::ApiFeature::Meta) == 4);
static_assert(static_cast<uint32_t>(PRISMA_UI_FLAT_API::ApiFeature::View) == 5);
static_assert(static_cast<uint32_t>(PRISMA_UI_FLAT_API::ApiFeature::Interop) == 6);
static_assert(static_cast<uint32_t>(PRISMA_UI_FLAT_API::ApiFeature::Localization) == 7);
static_assert(static_cast<uint32_t>(PRISMA_UI_FLAT_API::ApiFeature::Render) == 8);
static_assert(static_cast<uint32_t>(PRISMA_UI_FLAT_API::ApiFeature::Input) == 9);
static_assert(static_cast<uint32_t>(PRISMA_UI_FLAT_API::ApiFeature::Menu) == 10);
static_assert(PRISMA_UI_FLAT_API::MetaApiVersion == 1);
static_assert(PRISMA_UI_FLAT_API::ViewApiVersion == 1);
static_assert(PRISMA_UI_FLAT_API::InteropApiVersion == 1);
static_assert(PRISMA_UI_FLAT_API::LocalizationApiVersion == 1);
static_assert(PRISMA_UI_FLAT_API::RenderApiVersion == 1);
static_assert(PRISMA_UI_FLAT_API::InputApiVersion == 1);
static_assert(PRISMA_UI_FLAT_API::MenuApiVersion == 1);
static_assert(static_cast<uint32_t>(PRISMA_UI_FLAT_API::ApiFeature::Callbacks) == 11);
static_assert(PRISMA_UI_FLAT_API::CallbacksApiVersion == 1);

#ifdef _WIN32
void* probe_request_legacy() { return PRISMA_UI_API::RequestPluginAPI(); }
bool probe_flat_api() {
    PRISMA_UI_FLAT_API::CoreAPI table{};
    return PRISMA_UI_FLAT_API::GetAPI(
        PRISMA_UI_FLAT_API::ApiFeature::Core,
        PRISMA_UI_FLAT_API::CoreApiVersion,
        &table,
        sizeof(table));
}
void* probe_request_vr() { return PRISMA_UI_VR_API::RequestPluginVRAPI(); }
void* probe_request_v11() { return PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI11>(); }
void* probe_request_v12() { return PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI12>(); }
uint64_t probe_capabilities() { return PRISMA_UI_API::GetPrismaCapabilities(); }
bool probe_typed_discovery() {
    PRISMA_UI_FLAT_API::ViewAPI table{};
    return PRISMA_UI_FLAT_API::Discover<PRISMA_UI_FLAT_API::ApiFeature::View>(
        PRISMA_UI_FLAT_API::ViewApiVersion, table);
}
bool probe_typed_capability(const PRISMA_UI_FLAT_API::MetaAPI& meta) {
    return PRISMA_UI_FLAT_API::HasCapability(meta, PRISMA_UI_FLAT_API::ApiFeature::View,
                                             PRISMA_UI_FLAT_API::ViewApiVersion);
}
#endif

int main() { return 0; }
