// example-f4se-plugin/src/keyhandler/keyhandler.cpp
#include "PCH.h"
#include "keyhandler.h"

KeyHandler* KeyHandler::GetSingleton()
{
    static KeyHandler instance;
    return &instance;
}

void KeyHandler::RegisterSink()
{
    // F4 input sink registration: push our BSInputEventUser into MenuControls::handlers.
    auto* menuControls = RE::MenuControls::GetSingleton();
    if (menuControls) {
        menuControls->handlers.push_back(GetSingleton());
        REX::INFO("KeyHandler: Registered into MenuControls::handlers.");
    } else {
        REX::CRITICAL("KeyHandler: MenuControls singleton is null - input sink not registered.");
    }
}

KeyHandlerEvent KeyHandler::Register(uint32_t bsButtonCode, KeyEventType eventType, KeyCallback callback)
{
    std::unique_lock lock(_mutex);
    KeyHandlerEvent handle = _nextHandle++;
    _handleMap[handle] = { bsButtonCode, eventType };

    auto& cbs = _registeredCallbacks[bsButtonCode];
    if (eventType == KeyEventType::KEY_DOWN) {
        cbs.down[handle] = std::move(callback);
    } else {
        cbs.up[handle] = std::move(callback);
    }
    return handle;
}

void KeyHandler::Unregister(KeyHandlerEvent handle)
{
    std::unique_lock lock(_mutex);
    auto it = _handleMap.find(handle);
    if (it == _handleMap.end()) return;

    auto& cbs = _registeredCallbacks[it->second.key];
    if (it->second.type == KeyEventType::KEY_DOWN) {
        cbs.down.erase(handle);
    } else {
        cbs.up.erase(handle);
    }
    _handleMap.erase(it);
}

bool KeyHandler::ShouldHandleEvent(const RE::InputEvent* a_event)
{
    if (!a_event) return false;
    // Only care about button events from keyboard
    return a_event->eventType == RE::INPUT_EVENT_TYPE::kButton &&
           a_event->device == RE::INPUT_DEVICE::kKeyboard;
}

void KeyHandler::OnButtonEvent(const RE::ButtonEvent* a_event)
{
    if (!a_event) return;

    // BS_BUTTON_CODE values match Windows VK codes for keyboard.
    const uint32_t key = static_cast<uint32_t>(a_event->GetBSButtonCode());

    std::shared_lock lock(_mutex);
    auto it = _registeredCallbacks.find(key);
    if (it == _registeredCallbacks.end()) return;

    // QJustPressed(): value != 0 && heldDownSecs == 0  => first frame of key press (DOWN)
    // value == 0 => released (UP)
    if (a_event->QJustPressed()) {
        for (auto& [handle, cb] : it->second.down) cb();
    } else if (a_event->QAnalogValue() == 0.0F) {
        for (auto& [handle, cb] : it->second.up) cb();
    }
}
