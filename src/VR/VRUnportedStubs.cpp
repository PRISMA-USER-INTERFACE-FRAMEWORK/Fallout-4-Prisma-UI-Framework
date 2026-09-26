#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "PCH.h"
#include "PrismaUI/ActorPreviewSpike.h"
#include "PrismaUI/CameraSpike.h"
#include "PrismaUI/WebCompositor.h"
#include "PrismaUI/LocalMapSpike.h"
#include "PrismaUI/ModelPreview.h"
#include "PrismaUI/TextureOverlay.h"
#include "PrismaUI/VanillaUISuppressor.h"
#include "PrismaUI/KnownMenusLog.h"

namespace PrismaUI::VanillaUISuppressor {
    namespace {
        std::mutex g_menuMutex;
        std::unordered_set<std::string> g_suppressedMenus;
        std::unordered_map<std::string, PRISMA_UI_API::MenuSuppressPredicate> g_conditionalMenus;
        std::atomic<bool> g_menuSinkInstalled{false};

        void ApplyMenuVisibility(const RE::BSFixedString& a_name, bool a_visible) {
            auto* ui = RE::UI::GetSingleton();
            if (!ui) return;
            auto menu = ui->GetMenu(a_name);
            if (!menu || !menu->uiMovie) return;
            menu->uiMovie->SetVisible(a_visible);
        }

        void ForceCloseMenuNow(const RE::BSFixedString& a_name) {
            ApplyMenuVisibility(a_name, false);
            if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
                queue->AddMessage(a_name, RE::UI_MESSAGE_TYPE::kHide);
            }
        }

        class VanillaMenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
        public:
            RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event,
                                                  RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
                if (!a_event.opening) return RE::BSEventNotifyControl::kContinue;

                bool suppress = false;
                PRISMA_UI_API::MenuSuppressPredicate predicate = nullptr;
                bool firstSeen = false;
                {
                    std::lock_guard lock{g_menuMutex};
                    static std::set<std::string> s_seenMenus;
                    const std::string menuName{a_event.menuName.c_str()};
                    firstSeen = s_seenMenus.insert(menuName).second;
                    suppress = g_suppressedMenus.contains(menuName);
                    if (auto it = g_conditionalMenus.find(menuName); it != g_conditionalMenus.end()) {
                        predicate = it->second;
                    }
                }

                if (firstSeen) {
                    logger::info("VanillaUISuppressor[VR]: menu opened (first time this session): '{}'",
                                 a_event.menuName.c_str());
                }
                if (suppress) {
                    ApplyMenuVisibility(a_event.menuName, false);
                    logger::info("VanillaUISuppressor[VR]: reapplied suppression to '{}' on reopen",
                                 a_event.menuName.c_str());
                }

                if (predicate && predicate()) {
                    ForceCloseMenuNow(a_event.menuName);
                    logger::info("VanillaUISuppressor[VR]: predicate true, force-closed '{}' on open",
                                 a_event.menuName.c_str());
                }
                return RE::BSEventNotifyControl::kContinue;
            }

            static VanillaMenuSink* GetSingleton() {
                static VanillaMenuSink singleton;
                return std::addressof(singleton);
            }
        };
    }

    void Install() {
        if (g_menuSinkInstalled.load()) return;
        if (auto* ui = RE::UI::GetSingleton()) {
            ui->RegisterSink(VanillaMenuSink::GetSingleton());
            g_menuSinkInstalled.store(true);
            logger::info("VanillaUISuppressor[VR]: registered menu-open sink for auto-reapply");
        } else {
            logger::warn("VanillaUISuppressor[VR]: RE::UI singleton unavailable, menu auto-reapply not installed");
        }
    }

    bool SuppressHUDWidget(const char*, bool) { return false; }

    bool SuppressVanillaMenu(const char* a_menuName, bool a_suppress) {
        if (!a_menuName) return false;
        const RE::BSFixedString name{a_menuName};
        {
            std::lock_guard lock{g_menuMutex};
            if (a_suppress)
                g_suppressedMenus.emplace(a_menuName);
            else
                g_suppressedMenus.erase(a_menuName);
        }
        ApplyMenuVisibility(name, !a_suppress);
        logger::info("VanillaUISuppressor[VR]: SuppressVanillaMenu('{}', {})", a_menuName, a_suppress);
        if (a_suppress) {
            KnownMenus::LogMenuPolicyOnce("SuppressVanillaMenu", a_menuName, KnownMenus::Mechanism::Hide);
        }
        return true;
    }

    bool IsMenuSuppressed(const char* a_menuName) {
        if (!a_menuName) return false;
        std::lock_guard lock{g_menuMutex};
        return g_suppressedMenus.contains(a_menuName);
    }

    bool CloseVanillaMenu(const char* a_menuName) {
        if (!a_menuName) return false;
        auto* queue = RE::UIMessageQueue::GetSingleton();
        if (!queue) return false;
        queue->AddMessage(RE::BSFixedString{a_menuName}, RE::UI_MESSAGE_TYPE::kHide);
        logger::info("VanillaUISuppressor[VR]: CloseVanillaMenu('{}')", a_menuName);
        return true;
    }

    void SuppressVanillaMenuIf(const char* a_menuName, PRISMA_UI_API::MenuSuppressPredicate a_predicate) {
        if (!a_menuName) return;
        if (a_predicate) {
            KnownMenus::LogMenuPolicyOnce("SuppressVanillaMenuIf", a_menuName, KnownMenus::Mechanism::ConditionalClose);
        }
        std::lock_guard lock{g_menuMutex};
        if (a_predicate) {
            g_conditionalMenus[a_menuName] = a_predicate;
            logger::info("VanillaUISuppressor[VR]: SuppressVanillaMenuIf('{}') registered", a_menuName);
        } else {
            g_conditionalMenus.erase(a_menuName);
            logger::info("VanillaUISuppressor[VR]: SuppressVanillaMenuIf('{}') unregistered", a_menuName);
        }
    }

    void EnableActivateChoiceFilter(bool, bool) {}

    void SuppressActivateChoicePerk(std::uint32_t, bool) {}

    bool GetActivateChoiceLabel(std::uint32_t, std::string&) { return false; }

    bool TriggerActivateChoice(std::uint32_t) { return false; }
}

namespace PrismaUI::ModelPreview {
    bool Enabled() { return false; }

    void Show(ViewId, const std::string&) {}

    void Hide(ViewId, const std::string&) {}

    void TickCore(ID3D11Device*, ID3D11DeviceContext*) {}

    void GetOverlays(ViewId, std::vector<Overlay>& out) { out.clear(); }

    void GetActiveViews(std::vector<ViewId>& out) { out.clear(); }

    void DrainRemovedOverlayKeys(std::vector<uint64_t>&) {}

    bool HasPendingWork() { return false; }

    void SetViewGate(ViewGateFn) {}
    void SetGameLoadActive(bool) noexcept {}

    void SetStatusSink(StatusSink) {}

    void OnViewDestroyed(ViewId) {}

    void RequestMemorySample(const char*) {}

    void NoteViewCreated() {}

    void Shutdown() {}
}

namespace PrismaUI::TextureOverlay {
    void Show(ViewId, const std::string&) {}

    void Hide(ViewId, const std::string&) {}

    void GetOverlays(ViewId, std::vector<ModelPreview::Overlay>& out) { out.clear(); }

    void GetActiveViews(std::vector<ViewId>& out) { out.clear(); }

    void DrainRemovedOverlayKeys(std::vector<uint64_t>&) {}

    bool HasPendingWork() { return false; }

    void SetStatusSink(StatusSink) {}

    void OnViewDestroyed(ViewId) {}
}

namespace PrismaUI::ActorPreviewSpike {

    void Tick() {}
    void Shutdown() {}
}

namespace PrismaUI::LocalMapSpike {
    void Tick(ID3D11Device*, ID3D11DeviceContext*) {}
}

namespace PrismaUI::CameraSpike {
    void Tick(ID3D11Device*, ID3D11DeviceContext*) {}
}

namespace PrismaUI {
    bool WebCompositor::Init(ID3D11Device*, ID3D11DeviceContext*) { return false; }

    CompositeResult WebCompositor::Draw(IDXGISwapChain*, const Web::RenderTargetSnapshot&,
                                        const std::vector<ModelPreview::Overlay>&, int, int, bool) {
        return CompositeResult::NotInitialized;
    }

    CompositeResult WebCompositor::ComposeAndCommitLayers(const std::vector<Web::RenderTargetSnapshot>&,
                                                          const std::vector<ModelPreview::Overlay>&, uint32_t,
                                                          uint32_t) {
        return CompositeResult::NotInitialized;
    }

    Web::RenderTargetSnapshot WebCompositor::CommittedSnapshot() const { return {}; }

    void WebCompositor::InvalidateRenderTarget() {}

    void WebCompositor::DrawCursor(int, int, bool) {}

    void WebCompositor::DrawModelOverlays(const std::vector<ModelPreview::Overlay>&) {}
}
