#pragma once

#include "PrismaUI_F4_API.h"
#include "PrismaUI_F4_Modern_API.h"

#include <functional>

bool DispatchApiCallback(std::function<void()> task, PrismaView view = 0);

class PluginAPI
{
	using LatestInterface = PRISMA_UI_API::IVPrismaUI12;

public:
	class PrismaUIInterface : public LatestInterface
	{
	protected:
		PrismaUIInterface() noexcept {}
		virtual ~PrismaUIInterface() noexcept {}

	public:
		static PrismaUIInterface* GetSingleton() noexcept;

		virtual PrismaView CreateView(const char* htmlPath, PRISMA_UI_API::OnDomReadyCallback onDomReadyCallback = nullptr) noexcept override;
		virtual void Invoke(PrismaView view, const char* script, PRISMA_UI_API::JSCallback callback = nullptr) noexcept override;
		virtual void InteropCall(PrismaView view, const char* functionName, const char* argument) noexcept override;
		virtual void RegisterJSListener(PrismaView view, const char* fnName, PRISMA_UI_API::JSListenerCallback callback) noexcept override;
		virtual bool HasFocus(PrismaView view) noexcept override;
		virtual bool Focus(PrismaView view, bool pauseGame = false, bool disableFocusMenu = false) noexcept override;
		virtual void Unfocus(PrismaView view) noexcept override;
		virtual void Show(PrismaView view) noexcept override;
		virtual void Hide(PrismaView view) noexcept override;
		virtual bool IsHidden(PrismaView view) noexcept override;
		virtual int GetScrollingPixelSize(PrismaView view) noexcept override;
		virtual void SetScrollingPixelSize(PrismaView view, int pixelSize) noexcept override;
		virtual bool IsValid(PrismaView view) noexcept override;
		virtual void Destroy(PrismaView view) noexcept override;
		virtual void SetOrder(PrismaView view, int order) noexcept override;
		virtual int GetOrder(PrismaView view) noexcept override;
		virtual void CreateInspectorView(PrismaView view) noexcept override;
		virtual void SetInspectorVisibility(PrismaView view, bool visible) noexcept override;
		virtual bool IsInspectorVisible(PrismaView view) noexcept override;
		virtual void SetInspectorBounds(PrismaView view, float topLeftX, float topLeftY, unsigned int width, unsigned int height) noexcept override;
		virtual bool HasAnyActiveFocus() noexcept override;

		virtual void RegisterConsoleCallback(PrismaView view, PRISMA_UI_API::ConsoleMessageCallback callback) noexcept override;
		void RegisterConsoleCallbackFlat(PrismaView view, PRISMA_UI_FLAT_API::ConsoleMessageCallback callback) noexcept;

		virtual void RegisterTranslations(PrismaView view, const char* pluginName) noexcept override;

		virtual void BindUIEvent(PrismaView view, const char* functionName,
		                         PRISMA_UI_API::JSListenerCallback callback) noexcept override;

		virtual void EnumerateViews(PRISMA_UI_API::ViewEnumCallback callback,
		                            void* userdata) noexcept override;

		virtual void* GetViewSRV(PrismaView view) noexcept override;
		virtual void SetViewOffscreen(PrismaView view, bool offscreen) noexcept override;
		virtual bool BindViewToGeometry(PrismaView view, void* rootObject, const char* geometryName) noexcept override;
		virtual bool BindViewToScreenTexture(PrismaView view, void* rootObject, const char* textureSubstring) noexcept override;
		virtual void UnbindViewFromGeometry(PrismaView view) noexcept override;

		virtual bool SuppressHUDWidget(const char* className, bool suppress) noexcept override;
		virtual bool SuppressVanillaMenu(const char* menuName, bool suppress) noexcept override;
		virtual bool CloseVanillaMenu(const char* menuName) noexcept override;

		virtual void SuppressVanillaMenuIf(const char* menuName, PRISMA_UI_API::MenuSuppressPredicate predicate) noexcept override;
		virtual void EnableActivateChoiceFilter(bool enable, bool dropDefaultTake) noexcept override;
		virtual void SuppressActivateChoicePerk(uint32_t perkFormID, bool suppress) noexcept override;

		virtual void EnumerateViewsEx(PRISMA_UI_API::ViewEnumCallbackEx callback, void* userdata) noexcept override;

		virtual bool GetActivateChoiceLabel(uint32_t buttonIndex, char* outBuffer, size_t bufferSize) noexcept override;
		virtual bool TriggerActivateChoice(uint32_t buttonIndex) noexcept override;
		virtual PRISMA_UI_API::ViewHealth GetViewHealth(PrismaView view) noexcept override;
		virtual void SetViewOffscreenSize(PrismaView view, int width, int height) noexcept override;
		virtual void SetViewOffscreenBackground(PrismaView view, uint32_t argb) noexcept override;

		virtual bool IsUsingGamepad() noexcept override;
		virtual int  GetControllerStyle() noexcept override;
		virtual void SetControllerStyle(int style) noexcept override;
		virtual void NoteInputDevice(int device) noexcept override;
		virtual bool GetButtonPrompt(const char* userEvent, char* outBuffer, size_t bufferSize) noexcept override;
		virtual bool GetGamepadButtonName(uint32_t bsButtonCode, char* outBuffer, size_t bufferSize) noexcept override;
		virtual void SetViewOwnsEscape(PrismaView view, bool owns) noexcept override;

		virtual void SetViewRole(PrismaView view, PRISMA_UI_API::ViewRole role) noexcept override;
		virtual PRISMA_UI_API::ViewRole GetViewRole(PrismaView view) noexcept override;
		virtual PrismaView GetFocusedView() noexcept override;
		virtual bool IsAnyPanelVisible(PrismaView ignoreView) noexcept override;
		virtual bool FocusOverlay(PrismaView view, bool pauseGame = false, bool disableFocusMenu = false) noexcept override;
		virtual bool SetInputRegions(PrismaView view, const PRISMA_UI_API::InputRegion* regions, uint32_t count) noexcept override;

		virtual bool DispatchToGameThread(PRISMA_UI_API::GameThreadTaskCallback callback,
		                                  void* userdata) noexcept override;
		virtual bool IsGameThread() noexcept override;
		virtual bool BindGameThreadUIEvent(PrismaView view, const char* functionName,
		                                   PRISMA_UI_API::GameThreadUIEventCallback callback,
		                                   void* userdata) noexcept override;

		virtual bool BindControllerAction(PrismaView view, const char* canonicalButton,
		                                  const char* action) noexcept override;
		virtual bool UnbindControllerAction(PrismaView view, const char* canonicalButton) noexcept override;
		virtual void ClearControllerActions(PrismaView view) noexcept override;
		virtual PRISMA_UI_API::ControllerActionBridgeState GetControllerActionBridgeState(
		    PrismaView view) noexcept override;
		virtual bool BindControllerFocusEntry(PrismaView view, const char* canonicalButton,
		                                      PRISMA_UI_API::GameThreadTaskCallback callback,
		                                      void* userdata) noexcept override;
		virtual bool UnbindControllerFocusEntry(PrismaView view,
		                                        const char* canonicalButton) noexcept override;
		bool SetNativeGamepad(PrismaView view, bool enabled) noexcept;

	private:
		unsigned long apiTID = 0;
	};

	class VerifiedPrismaUIInterface final : public PrismaUIInterface
	{
	private:
		VerifiedPrismaUIInterface() noexcept = default;
		~VerifiedPrismaUIInterface() noexcept override = default;

	public:
		static VerifiedPrismaUIInterface* GetSingleton() noexcept
		{
			static VerifiedPrismaUIInterface singleton;
			return std::addressof(singleton);
		}

		PrismaView CreateView(const char* htmlPath,
		                      PRISMA_UI_API::OnDomReadyCallback onDomReadyCallback = nullptr) noexcept override;
		bool Focus(PrismaView view, bool pauseGame = false, bool disableFocusMenu = false) noexcept override;
		bool FocusOverlay(PrismaView view, bool pauseGame = false, bool disableFocusMenu = false) noexcept override;
		void Destroy(PrismaView view) noexcept override;

		void BindUIEvent(PrismaView view, const char* functionName,
		                 PRISMA_UI_API::JSListenerCallback callback) noexcept override;
	};

	static void DiagTickMeshBindings();
	static bool RegisterTranslationsV4(PrismaView view, const char* pluginName) noexcept;
};

inline PluginAPI::PrismaUIInterface* PluginAPI::PrismaUIInterface::GetSingleton() noexcept
{
	return PluginAPI::VerifiedPrismaUIInterface::GetSingleton();
}
