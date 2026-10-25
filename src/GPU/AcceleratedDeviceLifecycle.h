#pragma once

namespace PrismaUI::GPU {

struct AcceleratedLifecycleInputs {
    unsigned long long device = 0;
    unsigned long long immediateContext = 0;
    unsigned long long swapChain = 0;

    [[nodiscard]] bool operator==(const AcceleratedLifecycleInputs&) const noexcept = default;
};

class AcceleratedDeviceLifecycle final {
public:
    enum class Transition : unsigned char {
        Stable,
        Changed,
    };

    [[nodiscard]] unsigned long long Epoch() const noexcept { return epoch_; }
    [[nodiscard]] bool Dead() const noexcept { return dead_; }
    [[nodiscard]] bool ResizePending() const noexcept { return resizePending_; }
    [[nodiscard]] const AcceleratedLifecycleInputs& Inputs() const noexcept { return inputs_; }

    Transition Observe(AcceleratedLifecycleInputs inputs) noexcept {
        if (!initialized_) {
            initialized_ = true;
            inputs_ = inputs;
            dead_ = false;
            return Transition::Stable;
        }
        if (inputs == inputs_ && !dead_) return Transition::Stable;
        inputs_ = inputs;
        AdvanceEpoch();
        dead_ = false;
        resizePending_ = false;
        return Transition::Changed;
    }

    void BeginResize() noexcept { resizePending_ = true; }

    Transition CompleteResize(bool succeeded, AcceleratedLifecycleInputs inputs) noexcept {
        if (!resizePending_) return Transition::Stable;
        resizePending_ = false;
        if (!succeeded) return Transition::Stable;
        if (!initialized_) {
            initialized_ = true;
            inputs_ = inputs;
            dead_ = false;
            return Transition::Changed;
        }
        inputs_ = inputs;
        AdvanceEpoch();
        dead_ = false;
        return Transition::Changed;
    }

    void MarkDeviceLost() noexcept { dead_ = true; }

    [[nodiscard]] bool Accepts(unsigned long long epoch, AcceleratedLifecycleInputs inputs) const noexcept {
        return initialized_ && !dead_ && !resizePending_ && epoch == epoch_ && inputs == inputs_;
    }

private:
    void AdvanceEpoch() noexcept {
        ++epoch_;
        if (!epoch_) ++epoch_;
    }

    unsigned long long epoch_ = 1;
    AcceleratedLifecycleInputs inputs_{};
    bool initialized_ = false;
    bool dead_ = false;
    bool resizePending_ = false;
};

}
