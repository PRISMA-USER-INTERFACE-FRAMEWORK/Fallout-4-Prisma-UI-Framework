# `BindControllerAction`

**Since:** `IVPrismaUI12`

```cpp
virtual bool BindControllerAction(
    PrismaView view,
    const char* canonicalButton,
    const char* action
) noexcept = 0;
```

Maps one canonical controller button to an application action for a specific Prisma view.

## Parameters

- `view` - Target Prisma view. The view must be live and DOM-ready.
- `canonicalButton` - One of `A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `LS`, `RS`, `Back`, `Start`, `DUp`, `DDown`, `DLeft`, or `DRight`.
- `action` - Framework-owned copy of the application action identifier. Must contain 1-64 ASCII letters, digits, `_`, `-`, `.`, or `:`.

## Returns

Returns `true` when the mapping is accepted. Returns `false` for an invalid/not-ready view, an unknown canonical button, a null argument, or an invalid action identifier.

Binding the same button again for the same view replaces the previous action.

## Timing

Register controller actions from the view's DOM-ready callback. Calls made before DOM-ready are rejected so the controller bridge is installed in the final page context.

```cpp
static void OnDomReady(PrismaView view) {
    api->BindControllerAction(view, "X", "scrap");
    api->BindControllerAction(view, "Y", "favorite");
}
```

## Delivery

The focused page receives:

```js
window.addEventListener('prisma-controller-action', ({ detail }) => {
  // detail.action
  // detail.button
  // detail.state: pressed | repeat | released
});
```

Mapped input is admitted only for the exact focused live view. Admission can stop the original Fallout input event so it does not also activate the game underneath. This is asynchronous dispatch, not synchronous JavaScript acknowledgement.

## See also

- [Controller actions](../controller-actions.md)
- `UnbindControllerAction`
- `ClearControllerActions`
- `BindGameThreadUIEvent`
