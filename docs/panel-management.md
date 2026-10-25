---
id: panel-management
title: Panel Management
sidebar_label: Panel Management
sidebar_position: 12
---

# Panel Management

Panel management answers the question every multi-mod setup eventually hits: **is another Prisma UI already open, and should mine wait?**

This coordination surface requires `IVPrismaUI10`.

## View roles

```cpp
enum class ViewRole : uint32_t {
    kUnspecified = 0,
    kWidget = 1,
    kPanel = 2,
};
```

| Role | Use for | Counts as an interactive panel? |
|---|---|---|
| `kUnspecified` | undeclared/default view | No |
| `kWidget` | passive HUD/overlay | No |
| `kPanel` | inventory, crafting, MCM, terminal, dialog-like panel | Yes when visible/focused |

Declare a role on any view that takes input:

```cpp
g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
```

A focus-taking view left at `kUnspecified` logs a one-time framework warning and remains invisible to other plugins' `IsAnyPanelVisible` checks.

## `IsAnyPanelVisible`

Returns true if another view is focused, or is declared `kPanel` and is currently visible.

```cpp
if (g_api->IsAnyPanelVisible(g_view)) {
    return;
}

g_api->Show(g_view);
g_api->Focus(g_view, true, false);
```

Pass `0` to consider every Prisma view:

```cpp
const bool busy = g_api->IsAnyPanelVisible(0);
```

Passive `kWidget` views do not block this check.

## `GetFocusedView`

```cpp
PrismaView focused = g_api->GetFocusedView();
if (focused != 0 && focused != g_view) {
    logger::info("another Prisma view owns focus: {}", focused);
}
```

`HasAnyActiveFocus()` gives the older yes/no answer. `GetFocusedView()` identifies the owner.

## Normal panel focus

Use `Focus` when the whole view is intended to own normal Prisma input:

```cpp
void OpenPanel()
{
    if (!g_api || !g_api->IsValid(g_view)) return;
    if (g_api->IsAnyPanelVisible(g_view)) return;

    g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
    g_api->Show(g_view);
    g_api->Focus(g_view, true, false);
}
```

Close in the reverse order:

```cpp
g_api->Unfocus(g_view);
g_api->Hide(g_view);
```

## Selective overlay focus

V10 also exposes `SetInputRegions` and `FocusOverlay` for views where only selected rectangles should capture Prisma input.

This feature is capability-gated:

```cpp
if (!PRISMA_UI_API::HasPrismaCapability(
        PRISMA_UI_API::PrismaCapability::InputRegions)) {
    // Fall back to normal Focus() or disable the selective-overlay feature.
    return;
}

const PRISMA_UI_API::InputRegion regions[] = {
    { 80, 80, 420, 240 },
    { 560, 80, 180, 60 },
};

if (g_api->SetInputRegions(g_view, regions, 2)) {
    g_api->Show(g_view);
    g_api->FocusOverlay(g_view, false, false);
}
```

Clear regions with:

```cpp
g_api->SetInputRegions(g_view, nullptr, 0);
```

Do not assume V10 alone means this optional behavior is enabled. Use the capability helper.

## Escape ownership

If your focused panel should own Escape:

```cpp
g_api->SetViewOwnsEscape(g_view, true);
```

Only do this when the page has a working Escape handler that closes/unfocuses the panel. Otherwise you can trap the player inside your UI.

## Cooperative panel example

```cpp
static PRISMA_UI_API::IVPrismaUI10* g_api = nullptr;
static PrismaView g_view = 0;

void CreatePanel()
{
    if (!g_api || g_view) return;

    g_view = g_api->CreateView("MyPlugin/panel.html", OnDomReady);
    if (!g_view) return;

    g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
    g_api->Hide(g_view);
}

void TryOpenPanel()
{
    if (!g_api || !g_api->IsValid(g_view)) return;

    if (g_api->IsAnyPanelVisible(g_view)) {
        logger::debug("another Prisma panel is active; not opening");
        return;
    }

    g_api->Show(g_view);
    g_api->Focus(g_view, true, false);
}

void ClosePanel()
{
    if (!g_api || !g_api->IsValid(g_view)) return;
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
}
```

## Why roles matter

`EnumerateViews` intentionally sees passive and interactive views. Treating “any visible Prisma view” as a blocker would make an always-on HUD prevent every menu from opening.

Roles solve that:

- `kWidget` means “present but passive.”
- `kPanel` means “interactive screen that other cooperating plugins should respect.”
- `kUnspecified` is not counted and should not be left on a real input-taking panel.

## Related pages

- [`SetViewRole`](api/SetViewRole.md)
- [`GetViewRole`](api/GetViewRole.md)
- [`GetFocusedView`](api/GetFocusedView.md)
- [`IsAnyPanelVisible`](api/IsAnyPanelVisible.md)
- [`FocusOverlay`](api/FocusOverlay.md)
- [`SetInputRegions`](api/SetInputRegions.md)
- [View Lifecycle](view-lifecycle)
