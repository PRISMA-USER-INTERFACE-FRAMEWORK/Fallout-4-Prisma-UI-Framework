// example-f4se-plugin/src/keyhandler/keyhandler.h
//
// F4 input adaptation notes:
//   - F4's CommonLibF4 does NOT have BSInputDeviceManager::AddEventSink (that's Skyrim).
//   - Instead, F4 uses BSInputEventUser registered in MenuControls::handlers.
//   - Key codes are Windows virtual-key codes (BS_BUTTON_CODE in InputEvent.h).
//   - ButtonEvent::QJustPressed() => key down; QAnalogValue()==0 && QHeldDownSecs()>0 => key up.
#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <cstdint>

using KeyCallback     = std::function<void()>;
using KeyHandlerEvent = uint64_t;

inline constexpr KeyHandlerEvent INVALID_REGISTRATION_HANDLE = 0;

enum class KeyEventType : uint8_t { KEY_DOWN, KEY_UP };

struct CallbackInfo {
    uint32_t     key  = 0;
    KeyEventType type = KeyEventType::KEY_DOWN;
};

// Inherits from RE::BSInputEventUser and is registered into MenuControls::handlers.
class KeyHandler : public RE::BSInputEventUser
{
public:
    static KeyHandler* GetSingleton();

    // Call once during kGameDataReady (or kPostPostLoad) to hook into the input chain.
    static void RegisterSink();

    [[nodiscard]] KeyHandlerEvent Register(uint32_t bsButtonCode, KeyEventType eventType, KeyCallback callback);
    void Unregister(KeyHandlerEvent handle);

    // override (BSInputEventUser)
    bool ShouldHandleEvent(const RE::InputEvent* a_event) override;
    void OnButtonEvent(const RE::ButtonEvent* a_event) override;

private:
    KeyHandler()           = default;
    ~KeyHandler() override = default;
    KeyHandler(const KeyHandler&) = delete;
    KeyHandler& operator=(const KeyHandler&) = delete;

    struct KeyCallbacks {
        std::map<KeyHandlerEvent, KeyCallback> down;
        std::map<KeyHandlerEvent, KeyCallback> up;
    };

    std::map<uint32_t, KeyCallbacks>        _registeredCallbacks;
    std::map<KeyHandlerEvent, CallbackInfo> _handleMap;
    std::atomic<KeyHandlerEvent>            _nextHandle{ INVALID_REGISTRATION_HANDLE + 1 };
    std::shared_mutex                       _mutex;
};
