# `CreateView`

**Since:** `IVPrismaUI1`

```cpp
virtual PrismaView CreateView(
    const char* htmlPath,
    OnDomReadyCallback onDomReadyCallback = nullptr
) noexcept = 0;
```

Creates an HTML view and begins loading the specified file.

## Parameters

- `htmlPath` - Path relative to `Data/PrismaUI_F4/views/`, e.g. `"mymenu.html"` or
  `"Interface/MyPlugin/page.html"`. Use a subfolder named after your plugin to avoid filename
  collisions with other mods - MO2 merges every mod's `PrismaUI_F4/views/` folder into one virtual
  directory, so filenames must be unique across the whole load order.
- `onDomReadyCallback` - Optional. Called once on the **main game thread** when the DOM is fully
  parsed. Safe to call `RegisterJSListener` and `Invoke` from inside this callback.

## Returns

A non-zero `PrismaView` handle on success, or `0` on failure (e.g. `htmlPath` doesn't resolve to a
real file). The view starts **visible** - call `Hide(view)` immediately after creation unless you
want it on screen right away.

## Call timing

Create views on `kPostLoadGame` / `kNewGame`, not on `kGameDataReady`. The rendering system is not
ready until a game has actually loaded.

## Example

```cpp
static PrismaView g_view = 0;

void OnDomReady(PrismaView view) {
    logger::info("view {} DOM ready", view);
}

void CreateMyView() {
    g_view = g_api->CreateView("Interface/MyPlugin/menu.html", OnDomReady);
    g_api->Hide(g_view);   // views are visible by default; hide until the player opens it
}
```
