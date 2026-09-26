
        bool RetireSlot(int slot) {
            if (!PrismaUI::Hooks::RelaySlotReusable(
                    s_chainState[slot].load(std::memory_order_acquire) == ChainRetirementState::ForwardRelay))
                return false;
            if (ChainShadowVtable(slot).load(std::memory_order_acquire)) return RetireChainSlot(slot);
            const bool present = RemoveRetiredPresentSlot(slot);
            const bool resize = RemoveRetiredResizeSlot(slot);
            return present && resize;
        }

    PrismaUI::Hooks::HookCoverage D3DHooks::CaptureCoverage() {
            PrismaUI::Hooks::HookCoverage cov;
            cov.ourDetourA = reinterpret_cast<uintptr_t>(&HookPresentA);
            cov.ourDetourB = reinterpret_cast<uintptr_t>(&HookPresentB);
            cov.ourResizeDetourA = reinterpret_cast<uintptr_t>(&HookResizeBuffersA);
            cov.ourResizeDetourB = reinterpret_cast<uintptr_t>(&HookResizeBuffersB);
            cov.hookedTarget = reinterpret_cast<uintptr_t>(s_hookedPresentTarget.load(std::memory_order_acquire));
            cov.generation = s_generation.load(std::memory_order_acquire);
            cov.strategy = s_installStrategy.load(std::memory_order_acquire);
            const int presentSlot = s_activePresentSlot.load(std::memory_order_acquire);
            const int resizeSlot = s_activeResizeSlot.load(std::memory_order_acquire);
            cov.activePresentDetour = reinterpret_cast<uintptr_t>(PresentDetour(presentSlot));
            cov.activeResizeDetour = reinterpret_cast<uintptr_t>(ResizeDetour(resizeSlot));
            if (cov.strategy == PrismaUI::Hooks::HookInstallStrategy::SwapchainChain) {
                auto* swapChain = ChainSwapchain(presentSlot).load(std::memory_order_acquire);
                cov.coveredSwapChain = reinterpret_cast<uintptr_t>(swapChain);
                cov.chainOwned = swapChain && ChainOwnsDispatch(presentSlot, swapChain);
                cov.activePresentForwardable = ChainPreviousPresent(presentSlot).load(std::memory_order_acquire) != nullptr;
                cov.activeResizeForwardable = ChainPreviousResize(resizeSlot).load(std::memory_order_acquire) != nullptr;
            } else if (cov.strategy == PrismaUI::Hooks::HookInstallStrategy::DirectMinHook) {
                cov.activePresentForwardable = PresentTrampoline(presentSlot).load(std::memory_order_acquire) != nullptr;
                cov.activeResizeForwardable = ResizeTrampoline(resizeSlot).load(std::memory_order_acquire) != nullptr;
            }
            return cov;
        }

        bool EnsureMinHookInitialized() {
            const MH_STATUS init = MH_Initialize();
            if (init == MH_OK || init == MH_ERROR_ALREADY_INITIALIZED) return true;
            logger::critical("D3DHooks: MH_Initialize failed ({})", static_cast<int>(init));
            return false;
        }

    HRESULT APIENTRY D3DHooks::HookPresentA(REX::W32::IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
    {
        InFlightGuard guard{s_presentInFlightA};
        const PresentFunc fn = PrismaUI::Hooks::SelectHookForwarder(
            SlotStrategy(0).load(std::memory_order_acquire),
            ChainPreviousPresent(0).load(std::memory_order_acquire),
            s_presentTrampolineA.load(std::memory_order_acquire));
        if (!fn) {
            logger::critical("D3DHooks::HookPresentA entered without a trampoline");
            return E_FAIL;
        }
        if (!s_presentOrphanedA.load(std::memory_order_acquire) &&
            !ChainOrphaned(0).load(std::memory_order_acquire))
            PrismaUI::WebRuntime::OnPresent(reinterpret_cast<IDXGISwapChain*>(pSwapChain));
        return fn(pSwapChain, SyncInterval, Flags);
    }

    HRESULT APIENTRY D3DHooks::HookPresentB(REX::W32::IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
    {
        InFlightGuard guard{s_presentInFlightB};
        const PresentFunc fn = PrismaUI::Hooks::SelectHookForwarder(
            SlotStrategy(1).load(std::memory_order_acquire),
            ChainPreviousPresent(1).load(std::memory_order_acquire),
            s_presentTrampolineB.load(std::memory_order_acquire));
        if (!fn) {
            logger::critical("D3DHooks::HookPresentB entered without a trampoline");
            return E_FAIL;
        }
        if (!s_presentOrphanedB.load(std::memory_order_acquire) &&
            !ChainOrphaned(1).load(std::memory_order_acquire))
            PrismaUI::WebRuntime::OnPresent(reinterpret_cast<IDXGISwapChain*>(pSwapChain));
        return fn(pSwapChain, SyncInterval, Flags);
    }

    void D3DHooks::Install()
    {
        std::lock_guard lock{s_hookLifecycleMutex};

        auto* rendererData = RE::BSGraphics::GetRendererData();
        if (!rendererData || !rendererData->renderWindow[0].swapChain) {
            logger::critical("D3DHooks::Install: Failed to get IDXGISwapChain from BSGraphics!");
            return;
        }

        REX::W32::IDXGISwapChain* swapChain = rendererData->renderWindow[0].swapChain;
        const auto plan = ::PrismaUI::ConflictChecker::BuildHookInstallPlan(
            reinterpret_cast<IDXGISwapChain*>(swapChain), reinterpret_cast<void*>(&HookPresentA),
            reinterpret_cast<void*>(&HookPresentB), reinterpret_cast<void*>(&HookResizeBuffersA),
            reinterpret_cast<void*>(&HookResizeBuffersB));
        const auto strategy = plan.strategy;
        if (strategy == PrismaUI::Hooks::HookInstallStrategy::Reject) return;
        if (strategy == PrismaUI::Hooks::HookInstallStrategy::SwapchainChain) {
            if (!InstallChainSlot(0, swapChain, plan.present, plan.resize, plan.dispatch,
                                  plan.shadowSlots)) {
                logger::warn("D3DHooks::Install: swapchain chain publication failed; yielding hook coverage");
                return;
            }
            s_hookedPresentTarget.store(reinterpret_cast<void*>(ChainPreviousPresent(0).load(std::memory_order_acquire)),
                                        std::memory_order_release);
            s_hookedResizeTarget.store(reinterpret_cast<void*>(ChainPreviousResize(0).load(std::memory_order_acquire)),
                                       std::memory_order_release);
            s_activePresentSlot.store(0, std::memory_order_release);
            s_activeResizeSlot.store(0, std::memory_order_release);
            s_installStrategy.store(strategy, std::memory_order_release);
            const uint32_t generation = s_generation.fetch_add(1, std::memory_order_acq_rel) + 1;
            logger::info("coverage=active strategy=swapchain-chain shadowSlots={} generation={}",
                         plan.shadowSlots, generation);
            logger::info("D3D hooks installed: Present strategy=swapchain-chain shadowSlots={} previous={} ResizeBuffers strategy=swapchain-chain",
                         plan.shadowSlots, s_hookedPresentTarget.load(std::memory_order_acquire));
            return;
        }

        if (!EnsureMinHookInitialized()) return;

        if (!EnablePresentSlot(0, plan.present)) {
            logger::critical("D3DHooks::Install: Failed to hook IDXGISwapChain::Present!");
            return;
        }

        if (!EnableResizeSlot(0, plan.resize)) {
            logger::critical("D3DHooks::Install: Failed to hook IDXGISwapChain::ResizeBuffers!");
            RemoveRetiredPresentSlot(0);
            return;
        }

        s_presentTargetB.store(nullptr, std::memory_order_release);
        s_presentTrampolineB.store(nullptr, std::memory_order_release);
        s_resizeTargetB.store(nullptr, std::memory_order_release);
        s_resizeTrampolineB.store(nullptr, std::memory_order_release);
        s_activePresentSlot.store(0, std::memory_order_release);
        s_activeResizeSlot.store(0, std::memory_order_release);
        s_hookedPresentTarget.store(plan.present, std::memory_order_release);
        s_hookedResizeTarget.store(plan.resize, std::memory_order_release);
        SlotStrategy(0).store(strategy, std::memory_order_release);
        s_installStrategy.store(strategy, std::memory_order_release);
        s_generation.fetch_add(1, std::memory_order_acq_rel);

        logger::info("D3D hooks installed: Present(vtable[8]) + ResizeBuffers(vtable[13]) (A/B slots)");
    }

    void D3DHooks::Uninstall()
    {
        std::lock_guard lock{s_hookLifecycleMutex};

        bool allRetired = true;
        for (int slot = 0; slot < 2; ++slot) {
            if (!RetireSlot(slot)) allRetired = false;
        }

        if (!allRetired || s_minHookOrphaned.load(std::memory_order_acquire)) {

            logger::critical("D3DHooks::Uninstall: a detour was still in flight after drain; MinHook left "
                             "initialized and the live trampoline intact to avoid freeing in-flight state");
            return;
        }

        if (MH_Uninitialize() != MH_OK) {
            logger::warn("D3DHooks::Uninstall: MH_Uninitialize failed.");
        }
        s_hookedPresentTarget.store(nullptr, std::memory_order_release);
        s_hookedResizeTarget.store(nullptr, std::memory_order_release);
        s_installStrategy.store(PrismaUI::Hooks::HookInstallStrategy::Reject, std::memory_order_release);
        logger::info("D3D hooks uninstalled.");
    }

    PrismaUI::Hooks::HookCoverage D3DHooks::CurrentCoverage()
    {
        std::lock_guard lock{s_hookLifecycleMutex};
        return CaptureCoverage();
    }

    bool D3DHooks::OwnsActiveSwapchainChain(REX::W32::IDXGISwapChain* swapChain)
    {
        std::lock_guard lock{s_hookLifecycleMutex};
        if (s_installStrategy.load(std::memory_order_acquire) !=
            PrismaUI::Hooks::HookInstallStrategy::SwapchainChain)
            return false;
        const int presentSlot = s_activePresentSlot.load(std::memory_order_acquire);
        const int resizeSlot = s_activeResizeSlot.load(std::memory_order_acquire);
        return presentSlot == resizeSlot && ChainOwnsDispatch(presentSlot, swapChain);
    }

    bool D3DHooks::Reattach(void* newPresentTarget, void* newResizeTarget, int64_t nowMs,
                            uint32_t expectedGeneration)
    {
        std::lock_guard lock{s_hookLifecycleMutex};

        if (s_generation.load(std::memory_order_acquire) != expectedGeneration) {
            logger::warn("D3DHooks::Reattach: coverage generation changed before commit; skipping stale request");
            return false;
        }

        auto* rendererData = RE::BSGraphics::GetRendererData();
        auto* swapChain = rendererData ? rendererData->renderWindow[0].swapChain : nullptr;
        if (!swapChain) return false;
        const auto activeCoverage = CaptureCoverage();
        const auto activeStrategy = activeCoverage.strategy;
        const int activePresentSlot = s_activePresentSlot.load(std::memory_order_acquire);
        const int activeResizeSlot = s_activeResizeSlot.load(std::memory_order_acquire);
        const auto canUseActiveForwarder = [&](void* target, bool resize) {
            PrismaUI::Hooks::HookClassification classification;
            const auto detour = resize ? ResizeDetour(activeResizeSlot) : PresentDetour(activePresentSlot);
            ::PrismaUI::ConflictChecker::ClassifyHookTargetForInstall(target, &classification, detour);
            return !ChainOrphaned(activePresentSlot).load(std::memory_order_acquire) &&
                   PrismaUI::Hooks::CanUseActiveForwarder(
                       activeCoverage, resize, reinterpret_cast<uintptr_t>(swapChain),
                       reinterpret_cast<uintptr_t>(target), classification.jmpTarget);
        };
        const bool allowPresentForwarder = canUseActiveForwarder(newPresentTarget, false);
        const bool allowResizeForwarder = canUseActiveForwarder(newResizeTarget, true);
        const auto plan = ::PrismaUI::ConflictChecker::BuildHookInstallPlan(
            reinterpret_cast<IDXGISwapChain*>(swapChain),
            allowPresentForwarder ? nullptr : reinterpret_cast<void*>(&HookPresentA),
            allowPresentForwarder ? nullptr : reinterpret_cast<void*>(&HookPresentB),
            allowResizeForwarder ? nullptr : reinterpret_cast<void*>(&HookResizeBuffersA),
            allowResizeForwarder ? nullptr : reinterpret_cast<void*>(&HookResizeBuffersB));
        s_lastReattachAttemptMs.store(nowMs, std::memory_order_release);
        if (plan.present != newPresentTarget || plan.resize != newResizeTarget)
            return false;
        switch (plan.strategy) {
        case PrismaUI::Hooks::HookInstallStrategy::Reject:
            return false;
        case PrismaUI::Hooks::HookInstallStrategy::SwapchainChain: {
            const int oldSlot = s_activePresentSlot.load(std::memory_order_acquire);
            const int newSlot = oldSlot == 0 ? 1 : 0;
            if (!PrismaUI::Hooks::RelaySlotReusable(
                    s_chainState[newSlot].load(std::memory_order_acquire) == ChainRetirementState::ForwardRelay)) {
                logger::warn("D3DHooks::Reattach: relay capacity exhausted; yielding Prisma coverage safely");
                return false;
            }
            if (!RetireSlot(newSlot)) return false;
            if (!InstallChainSlot(newSlot, swapChain, newPresentTarget, newResizeTarget, plan.dispatch,
                                  plan.shadowSlots)) return false;
            s_hookedPresentTarget.store(reinterpret_cast<void*>(ChainPreviousPresent(newSlot).load(std::memory_order_acquire)),
                                        std::memory_order_release);
            s_hookedResizeTarget.store(reinterpret_cast<void*>(ChainPreviousResize(newSlot).load(std::memory_order_acquire)),
                                       std::memory_order_release);
            s_activePresentSlot.store(newSlot, std::memory_order_release);
            s_activeResizeSlot.store(newSlot, std::memory_order_release);
            s_installStrategy.store(PrismaUI::Hooks::HookInstallStrategy::SwapchainChain,
                                    std::memory_order_release);
            if (!RetireSlot(oldSlot))
                logger::warn("D3DHooks::Reattach: old hook slot {} retirement deferred", oldSlot);
            s_lastReattachMs.store(nowMs, std::memory_order_release);
            const uint32_t generation = s_generation.fetch_add(1, std::memory_order_acq_rel) + 1;
            logger::info("coverage=active strategy=swapchain-chain generation={}", generation);
            logger::warn("D3DHooks::Reattach: Present strategy=swapchain-chain generation={}", generation);
            return true;
        }
        case PrismaUI::Hooks::HookInstallStrategy::DirectMinHook:
            break;
        }

        void* oldPresent = s_hookedPresentTarget.load(std::memory_order_acquire);
        if (!newPresentTarget || (activeStrategy != PrismaUI::Hooks::HookInstallStrategy::SwapchainChain &&
                                  newPresentTarget == oldPresent))
            return false;

        if (!EnsureMinHookInitialized()) return false;

        const int oldPresentSlot = activePresentSlot;
        const int newPresentSlot = oldPresentSlot == 0 ? 1 : 0;
        void* oldResize = s_hookedResizeTarget.load(std::memory_order_acquire);
        const bool resizeMoves = activeStrategy == PrismaUI::Hooks::HookInstallStrategy::SwapchainChain ||
                                 (newResizeTarget && newResizeTarget != oldResize);
        const int oldResizeSlot = activeResizeSlot;
        const int newResizeSlot = oldResizeSlot == 0 ? 1 : 0;

        if (!RetireSlot(newPresentSlot)) {
            logger::warn("D3DHooks::Reattach: inactive Present slot {} is still retiring; skipping this attempt",
                         newPresentSlot);
            return false;
        }
        if (resizeMoves && !RetireSlot(newResizeSlot)) {
            logger::warn("D3DHooks::Reattach: inactive ResizeBuffers slot {} is still retiring; skipping this attempt",
                         newResizeSlot);
            return false;
        }

        if (!EnablePresentSlot(newPresentSlot, newPresentTarget)) {
            return false;
        }
        if (resizeMoves && !EnableResizeSlot(newResizeSlot, newResizeTarget)) {
            logger::warn("D3DHooks::Reattach: ResizeBuffers preparation failed; rolling back Present preparation");
            if (!RemoveRetiredPresentSlot(newPresentSlot)) {
                logger::critical("D3DHooks::Reattach: Present rollback was deferred; preserving MinHook state");
            }
            return false;
        }

        s_hookedPresentTarget.store(newPresentTarget, std::memory_order_release);
        s_activePresentSlot.store(newPresentSlot, std::memory_order_release);
        SlotStrategy(newPresentSlot).store(PrismaUI::Hooks::HookInstallStrategy::DirectMinHook,
                                           std::memory_order_release);
        if (resizeMoves) {
            s_hookedResizeTarget.store(newResizeTarget, std::memory_order_release);
            s_activeResizeSlot.store(newResizeSlot, std::memory_order_release);
            SlotStrategy(newResizeSlot).store(PrismaUI::Hooks::HookInstallStrategy::DirectMinHook,
                                              std::memory_order_release);
        }
        s_installStrategy.store(PrismaUI::Hooks::HookInstallStrategy::DirectMinHook,
                               std::memory_order_release);

        if (activeStrategy == PrismaUI::Hooks::HookInstallStrategy::SwapchainChain) {
            if (!RetireSlot(oldPresentSlot))
                logger::warn("D3DHooks::Reattach: old chain slot {} remains a forward relay", oldPresentSlot);
        } else {
            if (oldPresent && !RemoveRetiredPresentSlot(oldPresentSlot))
                logger::warn("D3DHooks::Reattach: old Present slot {} retirement deferred", oldPresentSlot);
            if (resizeMoves && oldResize && !RemoveRetiredResizeSlot(oldResizeSlot))
                logger::warn("D3DHooks::Reattach: old ResizeBuffers slot {} retirement deferred", oldResizeSlot);
        }

        s_lastReattachMs.store(nowMs, std::memory_order_release);
        const uint32_t generation = s_generation.fetch_add(1, std::memory_order_acq_rel) + 1;
        logger::warn("D3DHooks::Reattach: Present hook moved to new target {} (slot {}, generation {})",
                     newPresentTarget, newPresentSlot, generation);
        return true;
    }
