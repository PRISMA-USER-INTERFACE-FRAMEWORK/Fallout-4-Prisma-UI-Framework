# PrismaUI Layout API and Layout Editor

The Layout API lets a PrismaUI view be moved, resized, hidden and restored by the player, and lets a plugin take part in that. The framework ships one editor for every view, so a mod does not have to write its own.

Feature id `Layout = 12`, table `LayoutAPI`, version `LayoutApiVersion = 1`. Discover it like every other Modern API table:

```cpp
PRISMA_UI_FLAT_API::LayoutAPI layout{};
if (PRISMA_UI_FLAT_API::Discover<PRISMA_UI_FLAT_API::ApiFeature::Layout>(PRISMA_UI_FLAT_API::LayoutApiVersion, layout)) {
    // layout.RegisterView(view, "My HUD", nullptr);
}
```

The table is append-only. A newer framework may add functions after the last one; an older framework simply does not advertise them.

## Model

- Every view created through the framework is tracked as soon as it exists. It appears in the editor even if its plugin never calls the Layout API, so the player can hide or move any panel.
- `RegisterView` is optional. It gives the view a readable title and a default rectangle, and marks it as cooperating.
- The framework's own boot, dock and editor views are never tracked.
- A view is identified across game sessions by its normalized view path (lower case, forward slashes). The owning module name is stored with it for information only.

## Coordinate space

Rectangles are in a virtual 1920 x 1080 stage with the origin at the top left, in floating point. The stage maps linearly onto the game window, so a layout stays proportional at any resolution. The default rectangle of a view is the whole stage.

The view's page is rendered at window size as before and is scaled into its rectangle by the compositor. Pages do not need to change. Mouse input is mapped back into page space by the framework.

Limits: width and height are clamped to 32..3840 x 32..2160 stage units, and at least 32 units of the rectangle must stay on the stage. Non-finite values are rejected.

## Visibility

Hiding from the editor is a separate player flag, not the plugin's `Hide`. While it is set the view is not drawn and is reported hidden. A plugin's `Show` cannot clear it. The flag is saved and restored. Resetting a view clears it.

## Ownership and permission

- A plugin may change only views owned by its own module. The owner is the module that created the view, resolved from the address of the calling code. A view whose creator cannot be resolved has no owner, so no plugin may change it. Calls on other views fail with `NotOwner`.
- Read calls (`EnumerateViews`, `GetRect`) work for any tracked view.
- The framework editor is part of the framework and is not restricted. It works on every tracked view.
- `SetEditMode` and `Save` and `Load` are open to any plugin. `ResetAll` is editor-only and not in the table.

## Functions

| Slot | Function | Meaning |
| --- | --- | --- |
| 0 | `RegisterView(view, title, defaultRect)` | Set a title and optional default rectangle (null keeps the whole stage). Owner only. |
| 1 | `UnregisterView(view)` | Remove the cooperating mark. The view stays tracked. Owner only. |
| 2 | `EnumerateViews(callback, userdata)` | Call back once per tracked view with a `LayoutViewInfo`. |
| 3 | `GetRect(view, out)` | Current rectangle. |
| 4 | `SetRect(view, rect)` | Move or resize. Owner only. Clamped. |
| 5 | `SetUserVisible(view, visible)` | Clear or set the player hide flag. Owner only. |
| 6 | `ResetView(view)` | Restore the default rectangle and clear the hide flag. Owner only. |
| 7 | `Save()` | Write the layout file now. |
| 8 | `Load()` | Read the layout file and re-apply it. |
| 9 | `SetEditMode(enabled)` | Open or close the framework editor. |
| 10 | `IsEditMode()` | Whether the editor is open. |
| 11 | `Subscribe(callback, userdata)` | Register for change notifications. Returns a token, 0 on failure. |
| 12 | `Unsubscribe(token)` | Remove a subscription. |

All functions are `noexcept` and return `LayoutResult` unless noted.

```
enum class LayoutResult : int32_t { Ok = 0, InvalidArgument = 1, UnknownView = 2, NotOwner = 3,
                                    Unavailable = 4, IoFailed = 5 };
struct LayoutRect { float x, y, width, height; };
struct LayoutViewInfo { uint32_t structSize; PrismaView view; const char* title; const char* owner;
                        const char* path; LayoutRect rect; LayoutRect defaultRect; uint32_t flags; };
struct LayoutChange { uint32_t structSize; uint32_t kind; PrismaView view; };
```

`flags`: bit 0 shown to the player (not hidden by the player), bit 1 hidden by the player, bit 2 registered by its plugin, bit 3 owned by the caller. Strings in `LayoutViewInfo` are valid only during the callback.

`LayoutChange.kind`: 1 rectangle, 2 visibility, 3 reset, 4 view tracked, 5 view gone, 6 edit mode, 7 layout loaded. `view` is 0 for kinds 6 and 7.

## Threading

Table functions may be called from the game thread. Change callbacks are delivered on the game thread through the framework's game-thread dispatcher. The enumeration callback runs on the calling thread before the call returns. File reads and writes happen on the calling thread of `Save` and `Load`.

## Persistence

File: `Documents/My Games/Fallout4/PrismaUI_F4/layout.json`, UTF-8, at most 256 KiB.

```json
{"version":1,"editor":{"snap":10,"grid":false},
 "views":{"interface/myhud/index.html":{"x":120,"y":40,"w":640,"h":360,"hidden":false,"owner":"MyHud.dll"}}}
```

- Only views that differ from their default are written.
- The schema is versioned. Unknown fields are ignored and are not written back. A file with a higher `version` is still read for the fields this version knows. Before the framework first overwrites such a file, or a file it refused to read, it copies the original to `layout.json.bak` and does not overwrite an existing backup. If the copy fails the save returns `IoFailed`.
- A file that is too large, malformed or lacks a `version` is ignored and the previous layout stays in effect.
- At most 256 views are remembered. Saved entries for views that are not currently open are dropped first to make room. Paths longer than 192 bytes are shortened.
- Writes go to a temporary file first and replace the real file in one step.
- The file is read once, when the first view is created or when `Load`, `Save`, `SetEditMode` or `RegisterView` is first called, whichever comes first, and applied to each view as it is created. An early `Save` never replaces a layout that has not been read yet.
- Saves from the editor and from `Save` are serialized.

## The editor

The editor is a framework view opened from the Prisma Dock (the Edit layout button) or by `SetEditMode(true)`. Closing the pause menu closes it.

- A list of every tracked view with an eye button: hidden views stay hidden and the choice is saved.
- Drag a view's frame to move it. Drag the corner and edge handles to resize.
- Snap to 0, 5, 10 or 20 units, optional grid.
- Undo and redo (Ctrl+Z, Ctrl+Y), Reset view, Reset all, Save and Close.
- Keyboard: Tab chooses the next view (Shift+Tab the previous). Enter cycles select, move and resize mode. In select mode Up and Down choose a view and Left and Right hide or show it. In move and resize mode the arrows change the rectangle by the snap step (Shift for ten steps). H or Space toggles the eye, Delete or R resets the view, Ctrl+Z and Ctrl+Y undo and redo, Ctrl+S saves, Escape leaves move or resize mode and then closes the editor.
- Controller: the D-pad, A and B arrive as Arrow, Enter and Escape, so the same Enter cycle applies: D-pad chooses, A moves into move and resize mode, Left and Right in select mode hide or show the view, B goes back and then closes.
- The look follows the system color scheme and respects reduced motion.

## In-page contract

Pages need nothing. A page that wants to react may listen for:

```js
window.addEventListener('prisma-layout-edit', e => { /* e.detail.editing is true or false */ });
```

The event fires on registered views when edit mode starts and stops.

## Not covered

- Virtual reality builds do not advertise the Layout table. Where the framework has no compositor, the dock hides the Edit layout button and `SetEditMode(true)` returns `Unavailable`.
- Model previews and texture overlays placed by a view are not moved with the view.
- Views drawn offscreen onto world geometry are not tracked for layout.
