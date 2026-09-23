#include "API.h"
#include "API/ConsoleMessageMapping.h"
#include "MeshBinding.h"
#include "PrismaUI/ControllerGlyphs.h"
#include "PrismaUI/GameThreadDispatcher.h"
#include "PrismaUI/WebRuntime.h"
#include "PrismaUI/Translations.h"
#include "PrismaUI/VanillaUISuppressor.h"

#include <d3d11.h>
#include <set>

#include <cstring>
#include <functional>
#include <utility>
#include <vector>

bool DispatchApiCallback(std::function<void()> task, PrismaView view) {
#ifndef PRISMAUI_FO4VR
        if (PrismaUI::GameThreadDispatcher::Dispatch(task, view)) return true;
#endif
        if (const auto* taskInterface = F4SE::GetTaskInterface()) {
            if (view != 0) {
                taskInterface->AddTask([task = std::move(task), view]() mutable {
                    if (PrismaUI::WebRuntime::IsValid(view)) task();
                });
            } else {
                taskInterface->AddTask(std::move(task));
            }
            return true;
        }
        logger::critical("PrismaUI API callback dropped because no game-thread dispatcher is available (view={})", view);
        return false;
    }
void PluginAPI::DiagTickMeshBindings()
{
	PrismaUI::MeshBinding::DiagTick();
}

void PluginAPI::PrismaUIInterface::RegisterJSListener(PrismaView view, const char* fnName, PRISMA_UI_API::JSListenerCallback callback) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view) || !fnName || !callback) {
        logger::warn("RegisterJSListener: invalid args — view={} fn={}", view, fnName ? fnName : "null");
        return;
    }

    logger::info("RegisterJSListener: View [{}] registering '{}'", view, fnName);

    std::string name = fnName;
    std::function<void(std::string)> callbackWrapper = [callback, view, name](const std::string& arg) {
        (void)DispatchApiCallback([targetCallback = callback, data = arg, view, name]() {

            logger::debug("RegisterJSListener fired: View [{}] '{}' — data: '{}'", view, name, data);
            targetCallback(data.c_str());
        }, view);
    };

    if (!PrismaUI::WebRuntime::IsActive()) {
        logger::warn("RegisterJSListener: Ultralight backend not active -- view [{}] fn '{}' not registered", view, fnName);
        return;
    }
    PrismaUI::WebRuntime::RegisterJSListener(view, fnName, callbackWrapper);
}

bool PluginAPI::PrismaUIInterface::HasFocus(PrismaView view) noexcept
{
    if (!view) {
        return false;
    }
    return PrismaUI::WebRuntime::HasFocus(view);
}

bool PluginAPI::PrismaUIInterface::Focus(PrismaView view, bool pauseGame, bool disableFocusMenu) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
        return false;
    }
    return PrismaUI::WebRuntime::Focus(view, pauseGame, disableFocusMenu);
}

bool PluginAPI::PrismaUIInterface::FocusOverlay(PrismaView view, bool pauseGame, bool disableFocusMenu) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
        return false;
    }
    return PrismaUI::WebRuntime::FocusOverlay(view, pauseGame, disableFocusMenu);
}

void PluginAPI::PrismaUIInterface::Unfocus(PrismaView view) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
        return;
    }
    PrismaUI::WebRuntime::Unfocus(view);
}

void PluginAPI::PrismaUIInterface::Show(PrismaView view) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
		return;
	}
	PrismaUI::WebRuntime::Show(view);
}

void PluginAPI::PrismaUIInterface::Hide(PrismaView view) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
		return;
	}
	PrismaUI::WebRuntime::Hide(view);
}

bool PluginAPI::PrismaUIInterface::IsHidden(PrismaView view) noexcept
{
	if (!view) {
		return true;
	}
	return PrismaUI::WebRuntime::IsHidden(view);
}

int PluginAPI::PrismaUIInterface::GetScrollingPixelSize(PrismaView view) noexcept
{
    if (!view) {
        return 0;
    }
    logger::warn("GetScrollingPixelSize: not yet implemented for the Ultralight backend -- view [{}]", view);
    return 0;
}

void PluginAPI::PrismaUIInterface::SetScrollingPixelSize(PrismaView view, int pixelSize) noexcept
{
    if (!view) {
        return;
    }
    logger::warn("SetScrollingPixelSize: not yet implemented for the Ultralight backend -- view [{}] pixelSize={} unaffected", view, pixelSize);
}

bool PluginAPI::PrismaUIInterface::IsValid(PrismaView view) noexcept
{
    if (!view) {
        return false;
    }
    return PrismaUI::WebRuntime::IsValid(view);
}

void PluginAPI::PrismaUIInterface::Destroy(PrismaView view) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
        return;
    }
    UnbindViewFromGeometry(view);
    PrismaUI::WebRuntime::Destroy(view);
}

void PluginAPI::PrismaUIInterface::SetOrder(PrismaView view, int order) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
		return;
	}
	PrismaUI::WebRuntime::SetOrder(view, order);
}

int PluginAPI::PrismaUIInterface::GetOrder(PrismaView view) noexcept
{
	if (!view) {
		return -1;
	}
	return PrismaUI::WebRuntime::GetOrder(view);
}

void PluginAPI::PrismaUIInterface::CreateInspectorView(PrismaView view) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
        return;
    }
    PrismaUI::WebRuntime::CreateInspectorView(view);
}

void PluginAPI::PrismaUIInterface::SetInspectorVisibility(PrismaView view, bool visible) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
        return;
    }
    PrismaUI::WebRuntime::SetInspectorVisibility(view, visible);
}

bool PluginAPI::PrismaUIInterface::IsInspectorVisible(PrismaView view) noexcept
{
    if (!view) {
        return false;
    }

    return PrismaUI::WebRuntime::IsInspectorVisible(view);
}

void PluginAPI::PrismaUIInterface::SetInspectorBounds(PrismaView view, float topLeftX, float topLeftY, unsigned int width, unsigned int height) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
        return;
    }

    PrismaUI::WebRuntime::SetInspectorBounds(view, topLeftX, topLeftY, width, height);
}

bool PluginAPI::PrismaUIInterface::HasAnyActiveFocus() noexcept
{
	return PrismaUI::WebRuntime::HasAnyActiveFocus();
}

namespace {
	template <typename Callback, typename Mapper>
	void RegisterConsoleCallbackImpl(PrismaView view, Callback callback, Mapper mapLevel) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
		return;
	}

	if (!callback) {
		logger::info("RegisterConsoleCallback: View [{}] unregistered JS console capture", view);
		PrismaUI::WebRuntime::RegisterConsoleCallback(view, nullptr);
		return;
	}

	logger::info("RegisterConsoleCallback: View [{}] registered JS console capture", view);

	auto wrappedCallback = [callback, view, mapLevel](int levelValue, std::string msg, std::string , int ) {
		PrismaUI::API::DispatchConsoleMessage(view, levelValue, std::move(msg), callback, mapLevel,
		                                      DispatchApiCallback);
	};
	PrismaUI::WebRuntime::RegisterConsoleCallback(view, wrappedCallback);
}

}

void PluginAPI::PrismaUIInterface::RegisterConsoleCallback(PrismaView view, PRISMA_UI_API::ConsoleMessageCallback callback) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
		return;
	}
	RegisterConsoleCallbackImpl(view, callback, PrismaUI::API::MapUltralightLogSeverity);
}

void PluginAPI::PrismaUIInterface::RegisterConsoleCallbackFlat(
	PrismaView view, PRISMA_UI_FLAT_API::ConsoleMessageCallback callback) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
		return;
	}
	RegisterConsoleCallbackImpl(view, callback, PrismaUI::API::MapFlatLogSeverity);
}

void PluginAPI::PrismaUIInterface::RegisterTranslations(PrismaView view, const char* pluginName) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view) || !pluginName || pluginName[0] == '\0') {
		logger::warn("[V3] RegisterTranslations: invalid args — view={}", view);
		return;
	}

	const std::string plugin = pluginName;
	const std::string lang = PrismaUI::Translations::DetectGameLanguage();
	const auto entries = PrismaUI::Translations::ParseTranslationFile(plugin, lang);

	if (entries.empty()) {

		logger::warn("[V3] RegisterTranslations: no translations loaded for '{}' (lang '{}') -- view [{}] "
		             "will render raw $KEYS. Expected Data\\Interface\\Translations\\{}_{}.txt",
		             plugin, lang, view, plugin, lang);
		return;
	}

	const std::string script = PrismaUI::Translations::BuildL10NScript(entries);
	if (script.empty()) {
		return;
	}

	PrismaUI::WebRuntime::Invoke(view, script, nullptr);
	logger::info("[V3] RegisterTranslations: injected {} entries (lang '{}') into view [{}] for '{}' -- "
	             "window.L10N and window.t() are now available to its JS",
	             entries.size(), lang, view, plugin);
}

bool PluginAPI::RegisterTranslationsV4(PrismaView view, const char* pluginName) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view) || !pluginName || pluginName[0] == '\0') {
        logger::warn("[V4] RegisterTranslationsV4: invalid args — view={}", view);
        return false;
    }

    const std::string plugin = pluginName;
    const std::string lang = PrismaUI::Translations::DetectGameLanguage();
    const auto entries = PrismaUI::Translations::ParseJsonTranslationFile(plugin, lang);
    if (entries.empty()) {
        logger::warn("[V4] RegisterTranslationsV4: no usable JSON translations for '{}' (lang '{}')", plugin, lang);
        return false;
    }

    const std::string script = PrismaUI::Translations::BuildV4L10NScript(entries, lang);
    if (script.empty() || !PrismaUI::WebRuntime::IsValid(view)) return false;
    PrismaUI::WebRuntime::RegisterLocalizationScript(view, script);
    logger::info("[V4] RegisterTranslationsV4: registered {} entries (lang '{}') for view [{}] '{}'",
                 entries.size(), lang, view, plugin);
    return true;
}

void PluginAPI::PrismaUIInterface::BindUIEvent(PrismaView view, const char* functionName,
                                                PRISMA_UI_API::JSListenerCallback callback) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view) || !functionName || !callback) {
		logger::warn("[V4] BindUIEvent: invalid args — view={} fn={}", view, functionName ? functionName : "null");
		return;
	}

	logger::info("[V4] BindUIEvent: View [{}] registering game-thread listener '{}'", view, functionName);

	std::string fnName = functionName;
	auto wrapped = [callback, view, fnName](const std::string& arg) {
		(void)DispatchApiCallback([callback, view, fnName, arg]() {

			logger::debug("[V4] BindUIEvent fired: View [{}] '{}' — data: '{}'", view, fnName, arg);
			callback(arg.c_str());
		}, view);
	};

	PrismaUI::WebRuntime::RegisterJSListener(view, functionName, wrapped);
}

void PluginAPI::PrismaUIInterface::EnumerateViews(PRISMA_UI_API::ViewEnumCallback callback,
                                                   void* userdata) noexcept
{
	if (!callback) return;
	int count = 0;
	PrismaUI::WebRuntime::EnumerateViews(
		[&](PrismaUI::WebRuntime::ViewId id, const std::string& htmlPath, const std::string& owner) {
			logger::info("EnumerateViews: view [{}] path='{}' owner='{}'", id, htmlPath, owner);
			callback(id, htmlPath.c_str(), userdata);
			++count;
		});
	logger::info("EnumerateViews: {} view(s) enumerated", count);
}

void PluginAPI::PrismaUIInterface::EnumerateViewsEx(PRISMA_UI_API::ViewEnumCallbackEx callback,
                                                     void* userdata) noexcept
{
	if (!callback) return;
	int count = 0;
	PrismaUI::WebRuntime::EnumerateViews(
		[&](PrismaUI::WebRuntime::ViewId id, const std::string& htmlPath, const std::string& owner) {
			callback(id, htmlPath.c_str(), owner.c_str(), userdata);
			++count;
		});
	logger::info("EnumerateViewsEx: {} view(s) enumerated", count);
}

void* PluginAPI::PrismaUIInterface::GetViewSRV(PrismaView view) noexcept
{
	return PrismaUI::WebRuntime::GetViewSRV(view);
}

void PluginAPI::PrismaUIInterface::SetViewOffscreen(PrismaView view, bool offscreen) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view) || !PrismaUI::WebRuntime::IsValid(view)) return;
	if (!offscreen) UnbindViewFromGeometry(view);
	PrismaUI::WebRuntime::SetViewOffscreen(view, offscreen);
	logger::info("SetViewOffscreen: view [{}] offscreen={}", view, offscreen);
}

void PluginAPI::PrismaUIInterface::SetViewOffscreenSize(PrismaView view, int width, int height) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view) || width <= 0 || height <= 0) return;
	PrismaUI::WebRuntime::SetViewOffscreenSize(view, width, height);
	logger::info("SetViewOffscreenSize: view [{}] {}x{} (aspect {:.3f})", view, width, height,
	             static_cast<float>(width) / static_cast<float>(height));
}

void PluginAPI::PrismaUIInterface::SetViewOffscreenBackground(PrismaView view, uint32_t argb) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) return;
	PrismaUI::WebRuntime::SetViewOffscreenBackground(view, argb);
	logger::info("SetViewOffscreenBackground: view [{}] argb=0x{:08X}{}", view, argb,
	             argb == 0 ? " (transparent -- page can composite over its bound geometry)" : "");
}

bool PluginAPI::PrismaUIInterface::IsUsingGamepad() noexcept
{
	return PrismaUI::ControllerGlyphs::UsingGamepad();
}

int PluginAPI::PrismaUIInterface::GetControllerStyle() noexcept
{
	return PrismaUI::ControllerGlyphs::GetStyle() == PrismaUI::ControllerGlyphs::Style::kPlayStation ? 1 : 0;
}

void PluginAPI::PrismaUIInterface::SetControllerStyle(int style) noexcept
{
	PrismaUI::ControllerGlyphs::SetStyle(style == 1 ? PrismaUI::ControllerGlyphs::Style::kPlayStation
	                                                : PrismaUI::ControllerGlyphs::Style::kXbox);
}

void PluginAPI::PrismaUIInterface::NoteInputDevice(int device) noexcept
{
	switch (device) {
	case 0: PrismaUI::ControllerGlyphs::NoteInputDevice(RE::INPUT_DEVICE::kKeyboard); break;
	case 1: PrismaUI::ControllerGlyphs::NoteInputDevice(RE::INPUT_DEVICE::kMouse); break;
	case 2: PrismaUI::ControllerGlyphs::NoteInputDevice(RE::INPUT_DEVICE::kGamepad); break;
	default: break;
	}
}

bool PluginAPI::PrismaUIInterface::GetButtonPrompt(const char* userEvent, char* outBuffer, size_t bufferSize) noexcept
{
	if (!outBuffer || bufferSize == 0) return false;
	auto s = PrismaUI::ControllerGlyphs::ButtonPrompt(userEvent);
	if (s.empty()) return false;
	std::snprintf(outBuffer, bufferSize, "%s", s.c_str());
	return true;
}

bool PluginAPI::PrismaUIInterface::GetGamepadButtonName(uint32_t bsButtonCode, char* outBuffer, size_t bufferSize) noexcept
{
	if (!outBuffer || bufferSize == 0) return false;
	auto s = PrismaUI::ControllerGlyphs::CanonicalFromCode(bsButtonCode);
	if (s.empty()) return false;
	std::snprintf(outBuffer, bufferSize, "%s", s.c_str());
	return true;
}

void PluginAPI::PrismaUIInterface::SetViewOwnsEscape(PrismaView view, bool owns) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) return;
	PrismaUI::WebRuntime::SetViewOwnsEscape(view, owns);
}

void PluginAPI::PrismaUIInterface::SetViewRole(PrismaView view, PRISMA_UI_API::ViewRole role) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) return;
	PrismaUI::WebRuntime::SetViewRole(view, static_cast<uint32_t>(role));
	logger::info("SetViewRole: view [{}] role={}", view, static_cast<uint32_t>(role));
}

PRISMA_UI_API::ViewRole PluginAPI::PrismaUIInterface::GetViewRole(PrismaView view) noexcept
{
	if (!view) return PRISMA_UI_API::ViewRole::kUnspecified;
	return static_cast<PRISMA_UI_API::ViewRole>(PrismaUI::WebRuntime::GetViewRole(view));
}

PrismaView PluginAPI::PrismaUIInterface::GetFocusedView() noexcept
{
	return PrismaUI::WebRuntime::GetFocusedView();
}

bool PluginAPI::PrismaUIInterface::IsAnyPanelVisible(PrismaView ignoreView) noexcept
{
	return PrismaUI::WebRuntime::IsAnyPanelVisible(ignoreView);
}

bool PluginAPI::PrismaUIInterface::SetInputRegions(
    PrismaView view,
    const PRISMA_UI_API::InputRegion* regions,
    uint32_t count) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view) || (count > 0 && !regions)) return false;

    constexpr uint32_t kCap = static_cast<uint32_t>(PrismaUI::InputRegionPolicy::kMaxInputRegions);
    const uint32_t kept = count > kCap ? kCap : count;
    if (kept != count) {
        logger::warn("SetInputRegions: view [{}] passed {} regions, keeping the first {}", view, count, kept);
    }

    std::vector<PrismaUI::InputRegionPolicy::InputRegion> converted;
    try {
        converted.reserve(kept);
        for (uint32_t i = 0; i < kept; ++i) {
            converted.push_back({
                regions[i].x,
                regions[i].y,
                regions[i].width,
                regions[i].height
            });
        }
    } catch (const std::exception& e) {
        logger::error("SetInputRegions: view [{}] failed to build region list: {}", view, e.what());
        return false;
    }
    return PrismaUI::WebRuntime::SetInputRegions(view, converted.data(), kept);
}

bool PluginAPI::PrismaUIInterface::BindViewToGeometry(PrismaView view, void* rootObject, const char* geometryName) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view) || !rootObject || !geometryName) return false;
	if (!PrismaUI::MeshBinding::Enabled()) return false;
	auto* srv = static_cast<ID3D11ShaderResourceView*>(GetViewSRV(view));
	if (!srv) {
		logger::warn("BindViewToGeometry: view [{}] has no dedicated offscreen frame yet", view);
		return false;
	}
	return PrismaUI::MeshBinding::BindToGeometry(view, static_cast<RE::NiAVObject*>(rootObject),
	                                             geometryName, srv);
}

bool PluginAPI::PrismaUIInterface::BindViewToScreenTexture(PrismaView view, void* rootObject, const char* textureSubstring) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view) || !rootObject || !textureSubstring) return false;
	if (!PrismaUI::MeshBinding::Enabled()) return false;
	auto* srv = static_cast<ID3D11ShaderResourceView*>(GetViewSRV(view));
	if (!srv) return false;
	return PrismaUI::MeshBinding::BindToScreenTexture(view, static_cast<RE::NiAVObject*>(rootObject),
	                                                  textureSubstring, srv);
}

void PluginAPI::PrismaUIInterface::UnbindViewFromGeometry(PrismaView view) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) return;
	PrismaUI::MeshBinding::Unbind(view);
}

bool PluginAPI::PrismaUIInterface::SuppressHUDWidget(const char* className, bool suppress) noexcept
{
	return PrismaUI::VanillaUISuppressor::SuppressHUDWidget(className, suppress);
}

bool PluginAPI::PrismaUIInterface::SuppressVanillaMenu(const char* menuName, bool suppress) noexcept
{
	return PrismaUI::VanillaUISuppressor::SuppressVanillaMenu(menuName, suppress);
}

bool PluginAPI::PrismaUIInterface::CloseVanillaMenu(const char* menuName) noexcept
{
	return PrismaUI::VanillaUISuppressor::CloseVanillaMenu(menuName);
}

void PluginAPI::PrismaUIInterface::SuppressVanillaMenuIf(const char* menuName, PRISMA_UI_API::MenuSuppressPredicate predicate) noexcept
{
	PrismaUI::VanillaUISuppressor::SuppressVanillaMenuIf(menuName, predicate);
}

void PluginAPI::PrismaUIInterface::EnableActivateChoiceFilter(bool enable, bool dropDefaultTake) noexcept
{
	PrismaUI::VanillaUISuppressor::EnableActivateChoiceFilter(enable, dropDefaultTake);
}

void PluginAPI::PrismaUIInterface::SuppressActivateChoicePerk(uint32_t perkFormID, bool suppress) noexcept
{
	PrismaUI::VanillaUISuppressor::SuppressActivateChoicePerk(perkFormID, suppress);
}

bool PluginAPI::PrismaUIInterface::GetActivateChoiceLabel(uint32_t buttonIndex, char* outBuffer, size_t bufferSize) noexcept
{
	if (!outBuffer || bufferSize == 0) return false;

	std::string label;
	if (!PrismaUI::VanillaUISuppressor::GetActivateChoiceLabel(buttonIndex, label)) return false;

	strncpy_s(outBuffer, bufferSize, label.c_str(), _TRUNCATE);
	return true;
}

bool PluginAPI::PrismaUIInterface::TriggerActivateChoice(uint32_t buttonIndex) noexcept
{
	return PrismaUI::VanillaUISuppressor::TriggerActivateChoice(buttonIndex);
}

PRISMA_UI_API::ViewHealth PluginAPI::PrismaUIInterface::GetViewHealth(PrismaView view) noexcept
{
	if (!view) return PRISMA_UI_API::ViewHealth::kUnknown;

	int raw = PrismaUI::WebRuntime::GetViewHealth(view);
	switch (raw) {
	case static_cast<int>(PRISMA_UI_API::ViewHealth::kCreating):        return PRISMA_UI_API::ViewHealth::kCreating;
	case static_cast<int>(PRISMA_UI_API::ViewHealth::kDomReady):        return PRISMA_UI_API::ViewHealth::kDomReady;
	case static_cast<int>(PRISMA_UI_API::ViewHealth::kLive):            return PRISMA_UI_API::ViewHealth::kLive;
	case static_cast<int>(PRISMA_UI_API::ViewHealth::kLoadFailed):      return PRISMA_UI_API::ViewHealth::kLoadFailed;
	case static_cast<int>(PRISMA_UI_API::ViewHealth::kDomReadyTimeout): return PRISMA_UI_API::ViewHealth::kDomReadyTimeout;
	case static_cast<int>(PRISMA_UI_API::ViewHealth::kUnresponsive):    return PRISMA_UI_API::ViewHealth::kUnresponsive;
	case static_cast<int>(PRISMA_UI_API::ViewHealth::kJsError):         return PRISMA_UI_API::ViewHealth::kJsError;
	default:                                                            return PRISMA_UI_API::ViewHealth::kUnknown;
	}
}