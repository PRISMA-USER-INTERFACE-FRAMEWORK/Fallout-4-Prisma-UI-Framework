# Controller actions

This turns gamepad buttons into named actions for your page. You ask PrismaUI to map a button to an action, the page listens for the action, and the game underneath does not also receive that button press.

It saves you from installing your own raw Fallout controller sink just to catch X, Y, LB and RB.

## The short version

```cpp
// Request V12 once, at kGameDataReady.
g_prisma = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI12>();
if (!g_prisma) {
    REX::WARN("controller actions need PrismaUI V12");
}

// Bind after the page is ready.
static void OnDomReady(PrismaView view)
{
    if (!g_prisma) return;
    g_prisma->BindControllerAction(view, "X", "scrap");
    g_prisma->BindControllerAction(view, "Y", "favorite");
    g_prisma->BindControllerAction(view, "LB", "tab.prev");
    g_prisma->BindControllerAction(view, "RB", "tab.next");
}
```

```js
window.addEventListener('prisma-controller-action', function (event) {
  const { action, button, state } = event.detail;
  if (state !== 'pressed') return;

  if (action === 'scrap') scrapSelectedItem();
  if (action === 'favorite') toggleFavorite();
  if (action === 'tab.prev') previousTab();
  if (action === 'tab.next') nextTab();
});
```

That is the whole pattern. Everything below is detail.

## Which buttons you can map

`A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `LS`, `RS`, `Back`, `Start`,
`DUp`, `DDown`, `DLeft`, `DRight`.

`LS` and `RS` are stick clicks. Stick movement is a different feature — see [live stick data](#live-stick-data).

Action names are yours. They must be 1–64 characters from ASCII letters, digits, `_`, `-`, `.` and `:`. PrismaUI keeps its own copy, so a local `std::string` can go out of scope.

## What the page receives

One DOM event, always the same name:

```js
window.addEventListener('prisma-controller-action', function (event) {
  event.detail.action;   // "scrap"
  event.detail.button;   // "X"
  event.detail.state;    // "pressed" | "repeat" | "released"
});
```

Menus usually act on `pressed` only. List boxes that scroll on hold can use `repeat`. Games/emulators that need button-up use `released`.

## When an action is delivered

A mapped event reaches your page only when all of this is true:

- it is a gamepad button event;
- the view still exists;
- **that exact view has PrismaUI focus**;
- the button has a live mapping for that view.

When those hold, PrismaUI consumes the physical press so it does not also trigger the Fallout menu or gameplay underneath. Unmapped buttons, unfocused views, keyboard and mouse are untouched.

Consuming is asynchronous. The press was accepted for delivery to your page; it is not a synchronous JavaScript acknowledgement. Do not assume the page already handled it by the time the native call returns.

## Focus is the gate

No focus, no controller actions. If your panel is open but the game still has focus, nothing arrives. Make sure `Focus(view, pauseGame)` ran.

Two other notes:

- **Reloads reset focus expectations.** After a page reload, bind actions again from `OnDomReady`.
- **Other mods' panels.** If another PrismaUI panel takes focus, your actions stop arriving until focus comes back. That is correct.

## D-pad, A and B already work

PrismaUI gives every focused view basic navigation for free:

| Physical | Page receives |
| --- | --- |
| D-pad up/down/left/right | Arrow keys |
| A | Enter |
| B | Escape |

If you map one of these buttons yourself, your action **replaces** that synthetic-key behaviour for that button. It is never delivered twice.

This means you can start with ordinary keyboard-style navigation (arrow keys move the selection, Enter activates, Escape closes), and later map only the buttons you want as semantic actions — for example `X` for "scrap" — without breaking the rest.

## Triggers

`LT` and `RT` are analog. PrismaUI uses fixed thresholds so drift does not produce noisy presses:

- press at **0.55** or above;
- release at **0.45** or below;
- in between, the previous state is kept.

Trigger state is reset when focus is acquired, so returning to a view does not inherit a held trigger from the previous one.

## Live stick data

If the page needs stick positions or analog values (a radial menu, a cursor, a camera), enable native Gamepad mode on the view:

```cpp
g_controller.SetNativeGamepad(view, true);
```

```js
function poll() {
  const pads = navigator.getGamepads();
  const pad = pads && pads[0];
  if (pad) {
    const x = pad.axes[0], y = pad.axes[1];
    // move your cursor
  }
  requestAnimationFrame(poll);
}
```

Use native Gamepad for continuous values and `BindControllerAction` for discrete menu actions. Do not rebuild menu navigation out of raw button state.

## Button prompts

To draw a prompt that matches the player's controller style:

```cpp
char prompt[32]{};
if (g_prisma->GetButtonPrompt("Activate", prompt, sizeof(prompt))) {
    // Show prompt in the page, e.g. "A" on Xbox, "Cross" on PlayStation.
}
```

`GetButtonPrompt` and `GetGamepadButtonName` use the same canonical button names as `BindControllerAction`, so the prompt for a mapped button is consistent with the action you bound.

## Cleaning up

```cpp
g_prisma->UnbindControllerAction(g_view, "X");   // one button
g_prisma->ClearControllerActions(g_view);        // all buttons on this view
```

`Destroy(view)` clears the view's mappings. Long-lived views can bind once and keep the mappings until destruction; screens whose controls change (a list view versus a detail view) should clear and rebind when the mode changes.

## Runtime limits

Native controller conversion is validated for **Old-Gen 1.10.163** and **Anniversary Edition 1.11.240**. Other runtimes fail closed rather than installing guessed conversion hooks.

Fallout 4 VR does not advertise V11 or V12, so this interface does not exist there.

## Migrating an existing mod

If your mod already has its own controller sink just to catch a few buttons for a Prisma view:

1. Include `PrismaUI_F4_API.h` and request `IVPrismaUI12`.
2. Remove the duplicate sink for UI actions.
3. Bind actions in `OnDomReady`.
4. Listen for `prisma-controller-action` in the page.
5. Keep game-thread mutations behind `BindGameThreadUIEvent` / `DispatchToGameThread`.
6. Keep the prompt APIs for drawing glyphs.

This is the change that stops two mods from each consuming the same controller event.

## Common mistakes

- **Binding before DOM-ready.** The bind returns `false`; move it into `OnDomReady`.
- **Panel not focused.** Actions only arrive for the focused view.
- **Expecting Enter/Escape.** If you map `A`/`B`, map them to your own actions; otherwise the free Arrow/Enter/Escape navigation applies.
- **Rebuilding navigation from raw pad state.** Use actions for buttons, native Gamepad only for axes.
- **Forgetting VR.** V12 does not exist there; keep a keyboard/fallback path.

## Where to go next

- [A controller-friendly menu](cookbook/controller-menu) — complete recipe
- [`BindControllerAction`](api/BindControllerAction) · [`UnbindControllerAction`](api/UnbindControllerAction) · [`ClearControllerActions`](api/ClearControllerActions)
- [`GetButtonPrompt`](api/GetButtonPrompt) · [`GetGamepadButtonName`](api/GetGamepadButtonName)
- [Current API extensions](api-extensions) — what each interface adds
