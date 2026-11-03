---
title: A controller-friendly menu
sidebar_label: Controller menu
sidebar_position: 4
---

# A controller-friendly menu

This makes the panel from [Your first panel](../quick-start) usable with a gamepad: the D-pad moves the selection, **A** activates it, **B** closes the panel, and the page draws its own prompts.

PrismaUI maps the controller buttons you ask for into named actions. Your page receives those actions as DOM events, so the page never needs to know which physical button the player pressed.

## 1. Ask for the controller interface

`BindControllerAction` is part of `IVPrismaUI12`. Request that interface and keep it separate from the view interface:

```cpp
static PRISMA_UI_API::IVPrismaUI12* g_pad = nullptr;

case F4SE::MessagingInterface::kGameDataReady:
    g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI10>();
    g_pad = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI12>();
    if (!g_pad) {
        REX::WARN("controller actions need PrismaUI V12; falling back to keyboard");
    }
    break;
```

On a framework build without V12, `g_pad` is null. Check it before every call, or hide the controller UI when it is missing.

## 2. Bind the buttons after DOM-ready

```cpp
static void OnDomReady(PrismaView view)
{
    if (!g_pad) return;

    g_pad->BindControllerAction(view, "DUp",    "menu.up");
    g_pad->BindControllerAction(view, "DDown",  "menu.down");
    g_pad->BindControllerAction(view, "A",      "menu.accept");
    g_pad->BindControllerAction(view, "B",      "menu.close");
    g_pad->BindControllerAction(view, "LB",     "menu.previousTab");
    g_pad->BindControllerAction(view, "RB",     "menu.nextTab");
}
```

Button names PrismaUI understands: `A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `LS`, `RS`, `Back`, `Start`, `DUp`, `DDown`, `DLeft`, `DRight`.

Binding the same button twice replaces the previous action. Actions are only delivered to the focused view.

## 3. Handle the actions in the page

```js
const model = {
  index: 0,
  tabs: ['Weapons', 'Armor', 'Aid'],
  tab: 0,
};

function render() {
  document.getElementById('tab').textContent = model.tabs[model.tab];
  const rows = document.querySelectorAll('.row');
  rows.forEach((row, i) => row.classList.toggle('selected', i === model.index));
  document.getElementById('hint').textContent = 'A = select   B = close   LB/RB = tab';
}

window.addEventListener('prisma-controller-action', function (event) {
  const action = event.detail.action;
  const state = event.detail.state;
  if (state !== 'pressed') return;   // ignore 'repeat' and 'released'

  if (action === 'menu.up' && model.index > 0) model.index--;
  else if (action === 'menu.down') model.index++;
  else if (action === 'menu.previousTab') model.tab = (model.tab + model.tabs.length - 1) % model.tabs.length;
  else if (action === 'menu.nextTab') model.tab = (model.tab + 1) % model.tabs.length;
  else if (action === 'menu.accept') window.activateRow(model.index);
  else if (action === 'menu.close') window.requestClose();

  render();
});

// The plugin calls this through BindUIEvent.
window.requestClose = function () {
  window.closeRequested();
};

window.activateRow = function (index) {
  console.log('activate row ' + index);
};

render();
```

The plugin closes the view:

```cpp
g_api->BindUIEvent(view, "closeRequested", [](const char*) {
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
});
```

## 4. Open it the same way as before

```cpp
static void TogglePanel()
{
    if (!g_api || !g_api->IsValid(g_view)) return;

    g_visible = !g_visible;
    if (g_visible) {
        g_api->Show(g_view);
        g_api->Focus(g_view, /*pauseGame=*/true);   // pause: it is a menu
    } else {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    }
}
```

## Notes

- **Focus is required.** Mapped controller actions only arrive while your view is focused. If the panel is open but not focused, the game still receives the controller.
- **Unmapped buttons pass through.** D-pad, A and B that you did not bind keep their normal Fallout behaviour, so you can bind only what your menu needs.
- **Handle `state`.** You get `pressed`, `repeat` and `released`. Menus usually act on `pressed` only.
- **Controller prompts are yours to draw.** Keep the hint line in the page (the example above) or use the framework prompt helpers to render shared Xbox/PlayStation glyphs.
- **Replaces on rebind.** Calling `BindControllerAction` for the same button replaces the action for that view. Use `UnbindControllerAction` and `ClearControllerActions` when you rebuild a menu dynamically.
- **F12/DevTools and controller focus.** The inspector can take focus; when it closes the menu should still work. Test it once in game.

## Where to go next

- [A click-through HUD overlay](hud-overlay)
- [Controller actions](../controller-actions) for the full event and prompt model
- [API reference: BindControllerAction](../api/BindControllerAction)
