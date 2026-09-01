---
id: vanilla-ui-suppression
title: Vanilla UI Suppression
sidebar_label: Vanilla UI Suppression
sidebar_position: 8
---

# Vanilla UI Suppression

PrismaUI_F4 exposes native APIs for suppressing selected vanilla HUD widgets and menus. These APIs operate on Fallout's existing UI objects; they are separate from the Ultralight web runtime used to render Prisma views.

This page documents only methods present in the released **PrismaUI_F4 2.1.0** public header.

## Full-menu suppression

### `SuppressVanillaMenu`

```cpp
virtual bool SuppressVanillaMenu(const char* menuName, bool suppress) noexcept = 0;
```

Suppress or restore a full vanilla menu by its `MENU_NAME`.

```cpp
g_api->SuppressVanillaMenu("PipboyMenu", true);

// Later:
g_api->SuppressVanillaMenu("PipboyMenu", false);
```

Suppression is reapplied when that menu is created/opened again for as long as the framework's suppression state remains enabled for the name.

The public API does **not** expose an `IsMenuSuppressed` method in 2.1.0. Keep your own plugin state if you need to know whether your code requested suppression.

### `CloseVanillaMenu`

```cpp
virtual bool CloseVanillaMenu(const char* menuName) noexcept = 0;
```

Closes the menu now. It does not by itself prevent the menu from opening again later.

```cpp
g_api->CloseVanillaMenu("ContainerMenu");
```

If you need both immediate close and future suppression, combine it with `SuppressVanillaMenu(name, true)`.

### `SuppressVanillaMenuIf`

```cpp
typedef bool (*MenuSuppressPredicate)();

virtual void SuppressVanillaMenuIf(
    const char* menuName,
    MenuSuppressPredicate predicate) noexcept = 0;
```

Registers a predicate that is evaluated when the menu tries to open.

```cpp
static bool ShouldSuppressContainer()
{
    return g_replacementActive;
}

g_api->SuppressVanillaMenuIf(
    "ContainerMenu",
    &ShouldSuppressContainer);
```

Pass `nullptr` to unregister the predicate.

The predicate runs synchronously on the game thread. Keep it cheap and non-blocking.

## HUD widget suppression

`IVPrismaUI6` adds:

```cpp
virtual bool SuppressHUDWidget(
    const char* className,
    bool suppress) noexcept = 0;
```

Example:

```cpp
if (!g_api->SuppressHUDWidget("HUDCompass", true)) {
    logger::warn("HUDCompass suppression unavailable");
}
```

### Runtime support

The released 2.1.0 implementation supports its guarded HUD-widget path on:

- OG `1.10.163`
- AE `1.11.137+` when the required Address Library authority resolves

Unknown class names or unsupported/unresolved hook targets return `false` rather than patching a guessed address.

Do not repeat the older documentation that says this API is always OG-only.

## View replacement pattern

For an interactive replacement panel, combine vanilla suppression with normal Prisma panel coordination:

```cpp
static PRISMA_UI_API::IVPrismaUI10* g_api = nullptr;
static PrismaView g_panel = 0;

void OpenReplacement()
{
    if (!g_api || !g_api->IsValid(g_panel)) return;
    if (g_api->IsAnyPanelVisible(g_panel)) return;

    g_api->SuppressVanillaMenu("ContainerMenu", true);
    g_api->CloseVanillaMenu("ContainerMenu");

    g_api->SetViewRole(g_panel, PRISMA_UI_API::ViewRole::kPanel);
    g_api->Show(g_panel);
    g_api->Focus(g_panel, true, false);
}

void CloseReplacement()
{
    if (!g_api || !g_api->IsValid(g_panel)) return;

    g_api->Unfocus(g_panel);
    g_api->Hide(g_panel);
    g_api->SuppressVanillaMenu("ContainerMenu", false);
}
```

Your plugin should track whether *it* requested suppression and should clean up that state when its replacement feature is disabled.

## Activate-choice APIs

V7/V8 retain several activate-choice methods for ABI compatibility:

```cpp
virtual void EnableActivateChoiceFilter(
    bool enable,
    bool dropDefaultTake) noexcept = 0;

virtual void SuppressActivateChoicePerk(
    uint32_t perkFormID,
    bool suppress) noexcept = 0;

virtual bool GetActivateChoiceLabel(
    uint32_t buttonIndex,
    char* outBuffer,
    size_t bufferSize) noexcept = 0;

virtual bool TriggerActivateChoice(
    uint32_t buttonIndex) noexcept = 0;
```

### Actual 2.1.0 semantics

Do **not** describe this as a working generic “filter the vanilla button strip” API.

- `EnableActivateChoiceFilter` enables the capture path used by `GetActivateChoiceLabel` / `TriggerActivateChoice`.
- `dropDefaultTake` is retained in the ABI but is ignored in 2.1.0.
- `SuppressActivateChoicePerk` is an ABI placeholder; it logs/returns without implementing perk-row suppression.
- `GetActivateChoiceLabel` and `TriggerActivateChoice` operate on captured choices when capture is available.

If your feature depends on capture/trigger behavior, test it on every runtime family you advertise. Do not build a public feature around `SuppressActivateChoicePerk` or `dropDefaultTake` until the framework implements them.

## Menu event caveats

Vanilla menu operations can still produce normal game `MenuOpenCloseEvent` traffic. Your own event sink must avoid feedback loops when it closes a menu and then receives the resulting close event.

Keep replacement state in your plugin rather than assuming suppression eliminates vanilla events.

## Runtime/version guidance

PrismaUI_F4 2.1.0 supports desktop:

- OG `1.10.163`
- AE `1.11.137+` with matching Address Library data

The intermediate `1.10.980-1.10.984` Next-Gen line is deliberately rejected by the framework.

Engine-hook-backed features should be tested separately on OG and AE. A method existing in the ABI does not guarantee every engine-specific behavior is available on every runtime.

## Related API pages

- [`SuppressHUDWidget`](api/SuppressHUDWidget.md)
- [`SuppressVanillaMenu`](api/SuppressVanillaMenu.md)
- [`CloseVanillaMenu`](api/CloseVanillaMenu.md)
- [`SuppressVanillaMenuIf`](api/SuppressVanillaMenuIf.md)
- [`EnableActivateChoiceFilter`](api/EnableActivateChoiceFilter.md)
- [`SuppressActivateChoicePerk`](api/SuppressActivateChoicePerk.md)
- [`GetActivateChoiceLabel`](api/GetActivateChoiceLabel.md)
- [`TriggerActivateChoice`](api/TriggerActivateChoice.md)
- [Panel Management](panel-management)
