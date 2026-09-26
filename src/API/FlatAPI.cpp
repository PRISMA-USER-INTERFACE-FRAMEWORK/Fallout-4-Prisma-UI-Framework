#ifdef PRISMAUI_FLAT_API_HOST_TEST
#include "flat_api_host_api.h"
#else
#include "PCH.h"
#include "API.h"
#include "PrismaUI/WebInput.h"
#endif
#include "FlatAPI.h"
#include "PrismaUI/InputRegionPolicy.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace {
    namespace Legacy = PRISMA_UI_API;
    namespace Flat = PRISMA_UI_FLAT_API;

    PluginAPI::PrismaUIInterface* Api() noexcept
    {
        return PluginAPI::PrismaUIInterface::GetSingleton();
    }

    PrismaView CreateView(const char* path, Flat::OnDomReadyCallback callback) noexcept { return Api()->CreateView(path, callback); }
    void Invoke(PrismaView view, const char* script, Flat::JSCallback callback) noexcept { Api()->Invoke(view, script, callback); }
    void InteropCall(PrismaView view, const char* name, const char* argument) noexcept { Api()->InteropCall(view, name, argument); }
    void RegisterJSListener(PrismaView view, const char* name, Flat::JSListenerCallback callback) noexcept { Api()->RegisterJSListener(view, name, callback); }
    bool HasFocus(PrismaView view) noexcept { return Api()->HasFocus(view); }
    bool Focus(PrismaView view, bool pause, bool disable) noexcept { return Api()->Focus(view, pause, disable); }
    void Unfocus(PrismaView view) noexcept { Api()->Unfocus(view); }
    void Show(PrismaView view) noexcept { Api()->Show(view); }
    void Hide(PrismaView view) noexcept { Api()->Hide(view); }
    bool IsHidden(PrismaView view) noexcept { return Api()->IsHidden(view); }
    int GetScrollingPixelSize(PrismaView view) noexcept { return Api()->GetScrollingPixelSize(view); }
    void SetScrollingPixelSize(PrismaView view, int size) noexcept { Api()->SetScrollingPixelSize(view, size); }
    bool IsValid(PrismaView view) noexcept { return Api()->IsValid(view); }
    void Destroy(PrismaView view) noexcept { Api()->Destroy(view); }
    void SetOrder(PrismaView view, int order) noexcept { Api()->SetOrder(view, order); }
    int GetOrder(PrismaView view) noexcept { return Api()->GetOrder(view); }
    void CreateInspectorView(PrismaView view) noexcept { Api()->CreateInspectorView(view); }
    void SetInspectorVisibility(PrismaView view, bool visible) noexcept { Api()->SetInspectorVisibility(view, visible); }
    bool IsInspectorVisible(PrismaView view) noexcept { return Api()->IsInspectorVisible(view); }
    void SetInspectorBounds(PrismaView view, float x, float y, unsigned int width, unsigned int height) noexcept { Api()->SetInspectorBounds(view, x, y, width, height); }
    bool HasAnyActiveFocus() noexcept { return Api()->HasAnyActiveFocus(); }
    void RegisterConsoleCallback(PrismaView view, Flat::ConsoleMessageCallback callback) noexcept { Api()->RegisterConsoleCallbackFlat(view, callback); }
    void RegisterTranslations(PrismaView view, const char* plugin) noexcept { Api()->RegisterTranslations(view, plugin); }
    void BindUIEvent(PrismaView view, const char* name, Flat::JSListenerCallback callback) noexcept { Api()->BindUIEvent(view, name, callback); }
    void EnumerateViews(Flat::ViewEnumCallback callback, void* userdata) noexcept { Api()->EnumerateViews(callback, userdata); }
    void* GetViewSRV(PrismaView view) noexcept { return Api()->GetViewSRV(view); }
    void SetViewOffscreen(PrismaView view, bool offscreen) noexcept { Api()->SetViewOffscreen(view, offscreen); }
    bool BindViewToGeometry(PrismaView view, void* root, const char* name) noexcept { return Api()->BindViewToGeometry(view, root, name); }
    bool BindViewToScreenTexture(PrismaView view, void* root, const char* name) noexcept { return Api()->BindViewToScreenTexture(view, root, name); }
    void UnbindViewFromGeometry(PrismaView view) noexcept { Api()->UnbindViewFromGeometry(view); }
    bool SuppressHUDWidget(const char* name, bool suppress) noexcept { return Api()->SuppressHUDWidget(name, suppress); }
    bool SuppressVanillaMenu(const char* name, bool suppress) noexcept { return Api()->SuppressVanillaMenu(name, suppress); }
    bool CloseVanillaMenu(const char* name) noexcept { return Api()->CloseVanillaMenu(name); }
    void SuppressVanillaMenuIf(const char* name, Flat::MenuSuppressPredicate predicate) noexcept { Api()->SuppressVanillaMenuIf(name, predicate); }
    void EnableActivateChoiceFilter(bool enable, bool drop) noexcept { Api()->EnableActivateChoiceFilter(enable, drop); }
    void SuppressActivateChoicePerk(uint32_t id, bool suppress) noexcept { Api()->SuppressActivateChoicePerk(id, suppress); }
    void EnumerateViewsEx(Flat::ViewEnumCallbackEx callback, void* userdata) noexcept { Api()->EnumerateViewsEx(callback, userdata); }
    bool GetActivateChoiceLabel(uint32_t index, char* buffer, size_t size) noexcept { return Api()->GetActivateChoiceLabel(index, buffer, size); }
    bool TriggerActivateChoice(uint32_t index) noexcept { return Api()->TriggerActivateChoice(index); }
    Flat::ViewHealth GetViewHealth(PrismaView view) noexcept { return static_cast<Flat::ViewHealth>(Api()->GetViewHealth(view)); }
    void SetViewOffscreenSize(PrismaView view, int width, int height) noexcept { Api()->SetViewOffscreenSize(view, width, height); }
    void SetViewOffscreenBackground(PrismaView view, uint32_t argb) noexcept { Api()->SetViewOffscreenBackground(view, argb); }

    bool IsUsingGamepad() noexcept { return Api()->IsUsingGamepad(); }
    int GetControllerStyle() noexcept { return Api()->GetControllerStyle(); }
    void SetControllerStyle(int style) noexcept { Api()->SetControllerStyle(style); }
    void NoteInputDevice(int device) noexcept { Api()->NoteInputDevice(device); }
    bool GetButtonPrompt(const char* event, char* buffer, size_t size) noexcept { return Api()->GetButtonPrompt(event, buffer, size); }
    bool GetGamepadButtonName(uint32_t code, char* buffer, size_t size) noexcept { return Api()->GetGamepadButtonName(code, buffer, size); }
    void SetViewOwnsEscape(PrismaView view, bool owns) noexcept { Api()->SetViewOwnsEscape(view, owns); }
    void SetViewRole(PrismaView view, Flat::ViewRole role) noexcept { Api()->SetViewRole(view, static_cast<Legacy::ViewRole>(role)); }
    Flat::ViewRole GetViewRole(PrismaView view) noexcept { return static_cast<Flat::ViewRole>(Api()->GetViewRole(view)); }
    PrismaView GetFocusedView() noexcept { return Api()->GetFocusedView(); }
    bool IsAnyPanelVisible(PrismaView view) noexcept { return Api()->IsAnyPanelVisible(view); }
    bool FocusOverlay(PrismaView view, bool pause, bool disable) noexcept { return Api()->FocusOverlay(view, pause, disable); }
    bool SetInputRegions(PrismaView view, const Flat::InputRegion* regions, uint32_t count) noexcept
    {
        if (!regions || count == 0) return Api()->SetInputRegions(view, nullptr, count);
        constexpr auto kMaxInputRegions = PrismaUI::InputRegionPolicy::kMaxInputRegions;
        std::array<Legacy::InputRegion, kMaxInputRegions> converted{};
        const auto kept = std::min<size_t>(count, kMaxInputRegions);
        for (size_t i = 0; i < kept; ++i) {
            converted[i] = {regions[i].x, regions[i].y, regions[i].width, regions[i].height};
        }
        return Api()->SetInputRegions(view, converted.data(), count);
    }
    bool BindControllerAction(PrismaView view, const char* button, const char* action) noexcept { return Api()->BindControllerAction(view, button, action); }
    bool UnbindControllerAction(PrismaView view, const char* button) noexcept { return Api()->UnbindControllerAction(view, button); }
    void ClearControllerActions(PrismaView view) noexcept { Api()->ClearControllerActions(view); }
    Flat::ControllerActionBridgeState GetControllerActionBridgeState(PrismaView view) noexcept { return static_cast<Flat::ControllerActionBridgeState>(Api()->GetControllerActionBridgeState(view)); }
    bool BindControllerFocusEntry(PrismaView view, const char* button, Flat::GameThreadTaskCallback callback, void* userdata) noexcept { return Api()->BindControllerFocusEntry(view, button, callback, userdata); }
    bool UnbindControllerFocusEntry(PrismaView view, const char* button) noexcept { return Api()->UnbindControllerFocusEntry(view, button); }
    bool SetNativeGamepad(PrismaView view, bool enabled) noexcept { return Api()->SetNativeGamepad(view, enabled); }

    bool DispatchToGameThread(Flat::GameThreadTaskCallback callback, void* userdata) noexcept { return Api()->DispatchToGameThread(callback, userdata); }
    bool IsGameThread() noexcept { return Api()->IsGameThread(); }
    bool BindGameThreadUIEvent(PrismaView view, const char* name, Flat::GameThreadUIEventCallback callback, void* userdata) noexcept { return Api()->BindGameThreadUIEvent(view, name, callback, userdata); }

    const char* ProviderName() noexcept { return Flat::PrismaUIPluginName; }
    uint32_t ApiBuild() noexcept { return 1; }

    bool SupportedVersion(Flat::ApiFeature feature, uint32_t version) noexcept
    {
        switch (feature) {
        case Flat::ApiFeature::Core: return version == Flat::CoreApiVersion;
        case Flat::ApiFeature::Controller: return version == Flat::ControllerApiVersion;
        case Flat::ApiFeature::GameThread: return version == Flat::GameThreadApiVersion;
        case Flat::ApiFeature::Meta: return version == Flat::MetaApiVersion;
        case Flat::ApiFeature::View: return version == Flat::ViewApiVersion;
        case Flat::ApiFeature::Interop: return version == Flat::InteropApiVersion;
        case Flat::ApiFeature::Localization: return version == Flat::LocalizationApiVersion;
        case Flat::ApiFeature::Render: return version == Flat::RenderApiVersion;
        case Flat::ApiFeature::Input: return version == Flat::InputApiVersion;
        case Flat::ApiFeature::Menu: return version == Flat::MenuApiVersion;
        default: return false;
        }
    }

    bool MetaHasFeature(Flat::ApiFeature feature, uint32_t version) noexcept
    {
        return SupportedVersion(feature, version);
    }

    bool MetaHasFeatureVR(Flat::ApiFeature feature, uint32_t version) noexcept
    {
        if (feature == Flat::ApiFeature::GameThread) return false;
        return SupportedVersion(feature, version);
    }

#ifdef PRISMAUI_FLAT_API_HOST_TEST
    bool SetViewCursorPolicy(PrismaView view, Flat::CursorPolicy policy) noexcept
    {
        FlatApiHostTest::state.cursorView = view;
        FlatApiHostTest::state.cursorPolicy = policy;
        return view != 0;
    }

    Flat::CursorPolicy GetViewCursorPolicy(PrismaView view) noexcept
    {
        if (!view || FlatApiHostTest::state.cursorView != view) return Flat::CursorPolicy::Default;
        return FlatApiHostTest::state.cursorPolicy;
    }

    bool RegisterTranslationsV4(PrismaView view, const char* plugin) noexcept
    {
        FlatApiHostTest::state.translationV4View = view;
        return view != 0 && plugin != nullptr;
    }
#else
    bool SetViewCursorPolicy(PrismaView view, Flat::CursorPolicy policy) noexcept
    {
        return PrismaUI::WebInput::SetViewCursorPolicy(view, static_cast<uint32_t>(policy));
    }

    Flat::CursorPolicy GetViewCursorPolicy(PrismaView view) noexcept
    {
        return static_cast<Flat::CursorPolicy>(PrismaUI::WebInput::GetViewCursorPolicy(view));
    }

    bool RegisterTranslationsV4(PrismaView view, const char* plugin) noexcept
    {
        return PluginAPI::RegisterTranslationsV4(view, plugin);
    }
#endif

    template <class T>
    bool CopyTable(const T& source, uint32_t featureVersion, void* output, uint32_t outputSize,
                   uint32_t advertisedSize = static_cast<uint32_t>(sizeof(T))) noexcept
    {
        if (!output || outputSize < sizeof(uint32_t) * 2) return false;
        const uint32_t copyBytes = std::min<uint32_t>(
            advertisedSize, std::min<uint32_t>(outputSize, static_cast<uint32_t>(sizeof(T))));
        if (copyBytes < sizeof(uint32_t) * 2) return false;
        auto* bytes = static_cast<uint8_t*>(output);
        if (outputSize > copyBytes) std::memset(bytes + copyBytes, 0, outputSize - copyBytes);
        std::memcpy(bytes, &source, copyBytes);
        const uint32_t header[2] = { advertisedSize, featureVersion };
        std::memcpy(bytes, header, sizeof(header));
        return true;
    }

    const Flat::CoreAPI Core{
        sizeof(Flat::CoreAPI), 1, CreateView, Invoke, InteropCall, RegisterJSListener, HasFocus, Focus, Unfocus,
        Show, Hide, IsHidden, GetScrollingPixelSize, SetScrollingPixelSize, IsValid, Destroy, SetOrder, GetOrder,
        CreateInspectorView, SetInspectorVisibility, IsInspectorVisible, SetInspectorBounds, HasAnyActiveFocus,
        RegisterConsoleCallback, RegisterTranslations, BindUIEvent, EnumerateViews, GetViewSRV, SetViewOffscreen,
        BindViewToGeometry, BindViewToScreenTexture, UnbindViewFromGeometry, SuppressHUDWidget, SuppressVanillaMenu,
        CloseVanillaMenu, SuppressVanillaMenuIf, EnableActivateChoiceFilter, SuppressActivateChoicePerk,
        EnumerateViewsEx, GetActivateChoiceLabel, TriggerActivateChoice, GetViewHealth, SetViewOffscreenSize,
        SetViewOffscreenBackground};
    const Flat::ControllerAPI Controller{
        sizeof(Flat::ControllerAPI), 1, IsUsingGamepad, GetControllerStyle, SetControllerStyle, NoteInputDevice,
        GetButtonPrompt, GetGamepadButtonName, SetViewOwnsEscape, SetViewRole, GetViewRole, GetFocusedView,
        IsAnyPanelVisible, FocusOverlay, SetInputRegions, BindControllerAction, UnbindControllerAction,
        ClearControllerActions, GetControllerActionBridgeState, BindControllerFocusEntry, UnbindControllerFocusEntry,
        SetNativeGamepad};
    const Flat::ControllerAPI ControllerVR = [] {
        auto table = Controller;
        table.BindControllerFocusEntry = nullptr;
        table.UnbindControllerFocusEntry = nullptr;
        table.SetNativeGamepad = nullptr;
        return table;
    }();
    constexpr uint32_t kVRControllerTableSize =
        static_cast<uint32_t>(__builtin_offsetof(Flat::ControllerAPI, BindControllerFocusEntry));
    const Flat::GameThreadAPI GameThread{
        sizeof(Flat::GameThreadAPI), 1, DispatchToGameThread, IsGameThread, BindGameThreadUIEvent};

    const Flat::MetaAPI Meta{
        sizeof(Flat::MetaAPI), Flat::MetaApiVersion, ProviderName, ApiBuild, MetaHasFeature};
    const Flat::MetaAPI MetaVR{
        sizeof(Flat::MetaAPI), Flat::MetaApiVersion, ProviderName, ApiBuild, MetaHasFeatureVR};
    const Flat::ViewAPI View{
        sizeof(Flat::ViewAPI), Flat::ViewApiVersion, CreateView, Destroy, Show, Hide, Focus, Unfocus, HasFocus,
        IsValid, IsHidden, GetViewHealth, SetOrder, GetOrder, SetViewCursorPolicy, GetViewCursorPolicy};
    const Flat::InteropAPI Interop{
        sizeof(Flat::InteropAPI), Flat::InteropApiVersion, Invoke, InteropCall, RegisterJSListener, BindUIEvent,
        RegisterConsoleCallback};
    const Flat::LocalizationAPI Localization{
        sizeof(Flat::LocalizationAPI), Flat::LocalizationApiVersion, RegisterTranslations, RegisterTranslationsV4};
    const Flat::RenderAPI Render{
        sizeof(Flat::RenderAPI), Flat::RenderApiVersion, GetViewSRV, SetViewOffscreen, SetViewOffscreenSize,
        SetViewOffscreenBackground, BindViewToGeometry, BindViewToScreenTexture, UnbindViewFromGeometry};
    const Flat::InputAPI Input{
        sizeof(Flat::InputAPI), Flat::InputApiVersion, GetScrollingPixelSize, SetScrollingPixelSize, SetInputRegions};
    const Flat::MenuAPI Menu{
        sizeof(Flat::MenuAPI), Flat::MenuApiVersion, SuppressHUDWidget, SuppressVanillaMenu, CloseVanillaMenu,
        SuppressVanillaMenuIf, EnableActivateChoiceFilter, SuppressActivateChoicePerk, EnumerateViews,
        EnumerateViewsEx, GetActivateChoiceLabel, TriggerActivateChoice};
}

namespace PrismaUI::FlatAPI {
    bool Get(PRISMA_UI_FLAT_API::ApiFeature feature, uint32_t version, void* table, uint32_t tableSize) noexcept
    {
        if (!SupportedVersion(feature, version)) return false;
        switch (feature) {
        case PRISMA_UI_FLAT_API::ApiFeature::Core:
            return CopyTable(Core, PRISMA_UI_FLAT_API::CoreApiVersion, table, tableSize);
        case PRISMA_UI_FLAT_API::ApiFeature::Controller:
            return CopyTable(Controller, PRISMA_UI_FLAT_API::ControllerApiVersion, table, tableSize);
        case PRISMA_UI_FLAT_API::ApiFeature::GameThread:
            return CopyTable(GameThread, PRISMA_UI_FLAT_API::GameThreadApiVersion, table, tableSize);
        case PRISMA_UI_FLAT_API::ApiFeature::Meta:
            return CopyTable(Meta, PRISMA_UI_FLAT_API::MetaApiVersion, table, tableSize);
        case PRISMA_UI_FLAT_API::ApiFeature::View:
            return CopyTable(View, PRISMA_UI_FLAT_API::ViewApiVersion, table, tableSize);
        case PRISMA_UI_FLAT_API::ApiFeature::Interop:
            return CopyTable(Interop, PRISMA_UI_FLAT_API::InteropApiVersion, table, tableSize);
        case PRISMA_UI_FLAT_API::ApiFeature::Localization:
            return CopyTable(Localization, PRISMA_UI_FLAT_API::LocalizationApiVersion, table, tableSize);
        case PRISMA_UI_FLAT_API::ApiFeature::Render:
            return CopyTable(Render, PRISMA_UI_FLAT_API::RenderApiVersion, table, tableSize);
        case PRISMA_UI_FLAT_API::ApiFeature::Input:
            return CopyTable(Input, PRISMA_UI_FLAT_API::InputApiVersion, table, tableSize);
        case PRISMA_UI_FLAT_API::ApiFeature::Menu:
            return CopyTable(Menu, PRISMA_UI_FLAT_API::MenuApiVersion, table, tableSize);
        default: return false;
        }
    }

    bool GetVR(PRISMA_UI_FLAT_API::ApiFeature feature, uint32_t version, void* table, uint32_t tableSize) noexcept
    {
        if (feature == PRISMA_UI_FLAT_API::ApiFeature::GameThread) return false;
        if (feature == PRISMA_UI_FLAT_API::ApiFeature::Meta) {
            if (version != PRISMA_UI_FLAT_API::MetaApiVersion) return false;
            return CopyTable(MetaVR, PRISMA_UI_FLAT_API::MetaApiVersion, table, tableSize);
        }
        if (feature == PRISMA_UI_FLAT_API::ApiFeature::Controller) {
            if (version != PRISMA_UI_FLAT_API::ControllerApiVersion) return false;
            return CopyTable(ControllerVR, PRISMA_UI_FLAT_API::ControllerApiVersion, table, tableSize,
                             kVRControllerTableSize);
        }
        return Get(feature, version, table, tableSize);
    }
}
