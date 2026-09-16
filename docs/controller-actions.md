# Focused controller actions (V12)

`IVPrismaUI12` lets a mod map controller buttons to UI actions without installing its own raw Fallout controller sink. It derives from V11, so the existing V1-V11 ABI prefix remains unchanged.

> **Availability:** include the canonical `PrismaUI_F4_API.h` SDK header and request `IVPrismaUI12` at runtime. Always null-check the request. Older installed providers may expose only earlier interface versions.

## Public API

```cpp
virtual bool BindControllerAction(
    PrismaView view,
    const char* canonicalButton,
    const char* action) noexcept = 0;

virtual bool UnbindControllerAction(
    PrismaView view,
    const char* canonicalButton) noexcept = 0;

virtual void ClearControllerActions(PrismaView view) noexcept = 0;
```

`BindControllerAction` replaces the mapping for that view and button. It returns `false` until the target view is DOM-ready. `UnbindControllerAction` removes one mapping. `ClearControllerActions` removes every controller mapping owned by the view.

Action identifiers are copied into framework storage and must contain 1-64 ASCII letters, digits, `_`, `-`, `.`, or `:`.

## Canonical buttons

The accepted button names are:

`A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `LS`, `RS`, `Back`, `Start`, `DUp`, `DDown`, `DLeft`, `DRight`.

These match the canonical names used by `GetGamepadButtonName()` and Prisma controller glyph rendering. PlayStation mode changes presentation only. Your code still registers the canonical names above.

## Request V12

```cpp
#include "PrismaUI_F4_API.h"

static PRISMA_UI_API::IVPrismaUI12* g_prisma = nullptr;
static PrismaView g_view = 0;

// During/after kGameDataReady:
g_prisma = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI12>();
if (!g_prisma) {
    logger::warn("PrismaUI V12 unavailable; controller actions disabled");
}
```

Request the lowest API version your mod actually requires. If controller actions are optional, keep a fallback path when V12 is unavailable.

## Bind after DOM-ready

Controller actions should normally be registered from your DOM-ready callback. Binding before the final page context is ready returns `false`.

```cpp
static void OnDomReady(PrismaView view) {
    if (!g_prisma) return;

    g_prisma->BindControllerAction(view, "X", "scrap");
    g_prisma->BindControllerAction(view, "Y", "favorite");
    g_prisma->BindControllerAction(view, "LB", "category_prev");
    g_prisma->BindControllerAction(view, "RB", "category_next");
}
```

Duplicate registration for the same view/button replaces the previous action deterministically.

## JavaScript event

The focused page receives one stable browser event:

```js
window.addEventListener('prisma-controller-action', ({ detail }) => {
  console.log(detail.action); // registered action identifier
  console.log(detail.button); // canonical button name
  console.log(detail.state);  // pressed, repeat, or released
});
```

Example:

```js
window.addEventListener('prisma-controller-action', ({ detail }) => {
  if (detail.state !== 'pressed') return;

  switch (detail.action) {
    case 'scrap':
      scrapSelectedItem();
      break;
    case 'favorite':
      toggleFavorite();
      break;
    case 'category_prev':
      selectPreviousCategory();
      break;
    case 'category_next':
      selectNextCategory();
      break;
  }
});
```

Delivery runs in the view's Ultralight JavaScript context and is intended for UI work. If JavaScript needs to mutate Fallout-owned state, route that mutation through a native listener registered with V11 `BindGameThreadUIEvent`, or dispatch native work with `DispatchToGameThread`.

## Focus and event consumption

A mapped controller event is eligible only when:

- it is a gamepad button event;
- the mapped view still exists;
- that exact view owns PrismaUI focus; and
- the button has a live mapping for that view.

An admitted `pressed`, `repeat`, or `released` event is marked `kStop` on the Fallout input chain so the same physical input does not also activate a menu or gameplay action underneath the Prisma view.

`kStop` means the mapped event was accepted for asynchronous dispatch to the exact focused live view. It does **not** mean JavaScript synchronously acknowledged or handled the event.

Unmapped, unfocused, keyboard/mouse, idle, and invalid-view events are not consumed by the V12 controller-action path.

## Legacy D-pad, A, and B behavior

PrismaUI already provides automatic controller navigation for focused views:

- D-pad -> Arrow keys
- A -> Enter
- B -> Escape

That behavior remains unchanged when those buttons are **not** explicitly registered through V12.

If a view explicitly maps `A`, `B`, `DUp`, `DDown`, `DLeft`, or `DRight`, the V12 action replaces the legacy synthetic-key route for that button. One physical event is not delivered through both paths.

This makes migration incremental: map only the actions your UI needs and leave normal keyboard-style navigation alone.

## LT and RT hysteresis

Triggers are analog values, so V12 uses deterministic hysteresis:

- press at `0.55` or above;
- remain held through the middle band;
- release at `0.45` or below.

This avoids trigger drift creating noisy press/release edges. Trigger state is reset when focus is successfully acquired so returning to a view does not inherit stale trigger state.

`LS` and `RS` refer to the stick-click buttons. Thumbstick movement itself is outside this API.

## Lifecycle

Mappings are owned per `PrismaView`.

- `UnbindControllerAction` removes one button.
- `ClearControllerActions` removes all mappings for a view.
- `Destroy(view)` clears the view's mappings before backend destruction.
- A destroyed view cannot leak mappings into a later view.

For long-lived views, it is fine to bind once after DOM-ready and keep the mappings until destruction. For screens whose control scheme changes, rebind or clear mappings when the mode changes.

## Removing a mapping

```cpp
g_prisma->UnbindControllerAction(g_view, "X");
```

Clear all mappings:

```cpp
g_prisma->ClearControllerActions(g_view);
```

## Controller prompts

V12 handles actions, while the existing V9 prompt API handles presentation. Use them together:

```cpp
char prompt[32]{};
if (g_prisma->GetButtonPrompt("Activate", prompt, sizeof(prompt))) {
    // Show prompt in the page.
}
```

For a direct gamepad code, `GetGamepadButtonName()` returns the same canonical naming family used by V12.

## Migrating an existing mod

If your mod currently installs its own menu/player controller sink only to catch X/Y/LB/RB/etc. for a Prisma view:

1. Include `PrismaUI_F4_API.h` and request `IVPrismaUI12`.
2. Remove the duplicate raw controller sink for UI actions now owned by PrismaUI.
3. Bind actions after DOM-ready.
4. Handle `prisma-controller-action` in the page.
5. Keep V9 prompt/glyph APIs for visual button prompts.
6. Keep engine mutations behind the V11 game-thread APIs.

This avoids multiple mods independently consuming the same Fallout controller events.

## Fallout 4 VR

V12 follows the current V11 flat-provider boundary. The VR provider may compile shared controller policy, but it does not advertise V11 or V12. Do not assume the desktop V12 interface exists in Fallout 4 VR.

## See also

- [Current API extensions](api-extensions.md)
- [`BindControllerAction`](api/BindControllerAction.md)
- [`UnbindControllerAction`](api/UnbindControllerAction.md)
- [`ClearControllerActions`](api/ClearControllerActions.md)
- [`GetButtonPrompt`](api/GetButtonPrompt.md)
- [`GetGamepadButtonName`](api/GetGamepadButtonName.md)
- [`BindGameThreadUIEvent`](api/BindGameThreadUIEvent.md)
- [`DispatchToGameThread`](api/DispatchToGameThread.md)
