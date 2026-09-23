
    namespace {
        class InFlightGuard {
        public:
            explicit InFlightGuard(std::atomic<int>& counter) : counter_(counter) {
                counter_.fetch_add(1, std::memory_order_acq_rel);
            }
            ~InFlightGuard() {
                counter_.fetch_sub(1, std::memory_order_acq_rel);
            }
            InFlightGuard(const InFlightGuard&) = delete;
            InFlightGuard& operator=(const InFlightGuard&) = delete;
        private:
            std::atomic<int>& counter_;
        };

        bool IsDisableSuccess(MH_STATUS status) {
            return status == MH_OK || status == MH_ERROR_DISABLED || status == MH_ERROR_NOT_CREATED;
        }

        std::atomic<D3DHooks::PresentFunc>& PresentTrampoline(int slot) {
            return slot == 0 ? D3DHooks::s_presentTrampolineA : D3DHooks::s_presentTrampolineB;
        }
        std::atomic<void*>& PresentTarget(int slot) {
            return slot == 0 ? D3DHooks::s_presentTargetA : D3DHooks::s_presentTargetB;
        }
        std::atomic<int>& PresentInFlight(int slot) {
            return slot == 0 ? D3DHooks::s_presentInFlightA : D3DHooks::s_presentInFlightB;
        }
        std::atomic<bool>& PresentOrphaned(int slot) {
            return slot == 0 ? D3DHooks::s_presentOrphanedA : D3DHooks::s_presentOrphanedB;
        }
        LPVOID PresentDetour(int slot) {
            return slot == 0 ? reinterpret_cast<LPVOID>(&D3DHooks::HookPresentA)
                             : reinterpret_cast<LPVOID>(&D3DHooks::HookPresentB);
        }
        unsigned char* PresentPatch(int slot) {
            return slot == 0 ? D3DHooks::s_presentPatchA : D3DHooks::s_presentPatchB;
        }

        std::atomic<D3DHooks::ResizeBuffersFunc>& ResizeTrampoline(int slot) {
            return slot == 0 ? D3DHooks::s_resizeTrampolineA : D3DHooks::s_resizeTrampolineB;
        }
        std::atomic<void*>& ResizeTarget(int slot) {
            return slot == 0 ? D3DHooks::s_resizeTargetA : D3DHooks::s_resizeTargetB;
        }
        std::atomic<int>& ResizeInFlight(int slot) {
            return slot == 0 ? D3DHooks::s_resizeInFlightA : D3DHooks::s_resizeInFlightB;
        }
        std::atomic<bool>& ResizeOrphaned(int slot) {
            return slot == 0 ? D3DHooks::s_resizeOrphanedA : D3DHooks::s_resizeOrphanedB;
        }
        LPVOID ResizeDetour(int slot) {
            return slot == 0 ? reinterpret_cast<LPVOID>(&D3DHooks::HookResizeBuffersA)
                             : reinterpret_cast<LPVOID>(&D3DHooks::HookResizeBuffersB);
        }
        unsigned char* ResizePatch(int slot) {
            return slot == 0 ? D3DHooks::s_resizePatchA : D3DHooks::s_resizePatchB;
        }

        enum class RetirementResult { Removed, Orphaned, Deferred };
        enum class ChainRetirementState { Free, Active, DetachedDraining, ForwardRelay };
        constexpr size_t kPatchPrefixBytes = 8;
        constexpr size_t kPatchWindowBytes = 24;
        constexpr size_t kBaseSwapchainVtableSlots = 18;
        constexpr size_t kMaxSwapchainVtableSlots = 41;

        std::atomic<PrismaUI::Hooks::HookInstallStrategy> s_installStrategy{
            PrismaUI::Hooks::HookInstallStrategy::Reject};

        std::atomic<PrismaUI::Hooks::HookInstallStrategy> s_slotStrategy[2]{
            PrismaUI::Hooks::HookInstallStrategy::Reject,
            PrismaUI::Hooks::HookInstallStrategy::Reject};
        std::atomic<REX::W32::IDXGISwapChain*> s_chainSwapchain[2]{nullptr, nullptr};
        std::atomic<void**> s_chainOriginalVtable[2]{nullptr, nullptr};
        std::atomic<void**> s_chainShadowVtable[2]{nullptr, nullptr};
        std::atomic<D3DHooks::PresentFunc> s_chainPreviousPresent[2]{nullptr, nullptr};
        std::atomic<D3DHooks::ResizeBuffersFunc> s_chainPreviousResize[2]{nullptr, nullptr};
        std::atomic<bool> s_chainOrphaned[2]{false, false};
        std::atomic<ChainRetirementState> s_chainState[2]{ChainRetirementState::Free,
                                                           ChainRetirementState::Free};
        std::atomic<unsigned int> s_chainSlotCount[2]{0, 0};
        void* s_chainVtables[2][kMaxSwapchainVtableSlots]{};

        std::atomic<PrismaUI::Hooks::HookInstallStrategy>& SlotStrategy(int slot) {
            return s_slotStrategy[slot];
        }
        std::atomic<REX::W32::IDXGISwapChain*>& ChainSwapchain(int slot) {
            return s_chainSwapchain[slot];
        }
        std::atomic<void**>& ChainOriginalVtable(int slot) {
            return s_chainOriginalVtable[slot];
        }
        std::atomic<void**>& ChainShadowVtable(int slot) {
            return s_chainShadowVtable[slot];
        }
        std::atomic<D3DHooks::PresentFunc>& ChainPreviousPresent(int slot) {
            return s_chainPreviousPresent[slot];
        }
        std::atomic<D3DHooks::ResizeBuffersFunc>& ChainPreviousResize(int slot) {
            return s_chainPreviousResize[slot];
        }
        std::atomic<bool>& ChainOrphaned(int slot) {
            return s_chainOrphaned[slot];
        }
        std::atomic<unsigned int>& ChainSlotCount(int slot) {
            return s_chainSlotCount[slot];
        }
        void** ChainVtable(int slot) { return s_chainVtables[slot]; }
