#include "PrismaUI/UltralightGamepad.h"
#include <AppCore/Platform.h>
#include <Ultralight/Ultralight.h>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    using namespace ultralight;
    using namespace PrismaUI::Web;
    Config config;
    config.resource_path_prefix = String((std::string(argv[1]) + "/resources/").c_str());
    config.cache_path = argv[2];
    auto& platform = Platform::instance();
    platform.set_config(config);
    platform.set_font_loader(GetPlatformFontLoader());
    platform.set_file_system(GetPlatformFileSystem(argv[1]));
    auto renderer = Renderer::Create();
    ViewConfig viewConfig;
    viewConfig.is_accelerated = false;
    auto first = renderer->CreateView(320, 160, viewConfig, nullptr);
    auto second = renderer->CreateView(320, 160, viewConfig, renderer->CreateSession(false, "second"));
    auto pump = [&] {
        for (int i = 0; i < 30; ++i) {
            renderer->Update();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };
    for (auto* view : {first.get(), second.get()})
        view->LoadHTML("<!doctype html><button>Test</button><script>window.keyCount=0; window.clickCount=0; addEventListener('keydown',()=>++keyCount); addEventListener('click',()=>++clickCount); addEventListener('gamepadconnected',function(){}); navigator.getGamepads();</script>");
    pump();
    first->Focus();
    second->Unfocus();
    auto check = [&](const char* expression) {
        pump();
        for (auto* view : {first.get(), second.get()}) {
            const auto value = view->EvaluateScript(expression).utf8();
            if (std::string(value.data(), value.length()) != "true") {
                std::fprintf(stderr, "FAIL: %s -> %s\n", expression, value.data());
                return false;
            }
        }
        return true;
    };
    GamepadState pad, published;
    pad.Apply({GamepadInput::Type::Button, 0, 1, 0, true});
    PublishGamepad(*renderer, pad, published);
    if (!check("!!navigator.getGamepads()[0] && navigator.getGamepads()[0].buttons[0].pressed")) return 1;
    pad.Apply({GamepadInput::Type::Stick, 0, 0, 0});
    pad.Apply({GamepadInput::Type::Stick, 0, 0.625f, -0.375f});
    pad.Apply({GamepadInput::Type::Button, 6, 0.42f, 0, true});
    PublishGamepad(*renderer, pad, published);
    if (!check("(()=>{const p=navigator.getGamepads()[0];return p.axes[0]===0.625 && p.axes[1]===-0.375 && Math.abs(p.buttons[6].value-0.42)<0.00001 && keyCount===0 && clickCount===0})()")) return 1;
    pad = {};
    PublishGamepad(*renderer, pad, published);
    if (!check("!navigator.getGamepads()[0]")) return 1;
    second->Focus();
    first->Unfocus();
    pad.Apply({GamepadInput::Type::Connection, 0, 1});
    pad.Apply({GamepadInput::Type::Button, 0, 1});
    pad.Apply({GamepadInput::Type::Stick, 0, 0.625f, -0.375f});
    PublishGamepad(*renderer, pad, published);
    if (!check("(()=>{const p=navigator.getGamepads()[0];return !!p && p.axes.every(x=>x===0) && p.buttons.every(b=>b.value===0)})()")) return 1;
    pad.Apply({GamepadInput::Type::Stick, 0, 0, 0});
    pad.Apply({GamepadInput::Type::Stick, 0, -0.5f, 0.25f});
    pad.Apply({GamepadInput::Type::Button, 0, 0});
    pad.Apply({GamepadInput::Type::Button, 0, 1, 0, true});
    PublishGamepad(*renderer, pad, published);
    if (!check("navigator.getGamepads()[0].axes[0]===-0.5 && navigator.getGamepads()[0].buttons[0].pressed")) return 1;
    std::puts("Native SDK publication, two-view visibility, reset and reconnect passed.");
}
