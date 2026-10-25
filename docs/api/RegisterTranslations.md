# `RegisterTranslations`

**Since:** `IVPrismaUI3`

```cpp
virtual void RegisterTranslations(
    PrismaView view,
    const char* pluginName
) noexcept = 0;
```

Loads the current Fallout 4 translation file for `pluginName`, builds the `window.L10N` / `window.t()` helper script, and injects that script into the **current live document** through PrismaUI's JavaScript execution path.

## Parameters

- `view` - Target view handle.
- `pluginName` - Your plugin's base name without extension, for example `"MyPlugin_F4"`.

## Translation file location

`Data\Interface\Translations\<pluginName>_<lang>.txt`, where `<lang>` matches the detected game language such as `en`, `de`, or `fr`.

## Call timing

Call `RegisterTranslations` from the view's DOM-ready callback, before invoking application code that requires translated strings.

```cpp
static void OnDomReady(PrismaView view)
{
    g_api->RegisterTranslations(view, "MyPlugin_F4");
    g_api->Invoke(view, "window.onTranslationsReady && window.onTranslationsReady()");
}
```

Do **not** assume `window.t` exists in module-level JavaScript or in scripts that execute before DOM-ready. In 2.1.0 this API performs a runtime `Invoke`; it is not a pre-document/window injection hook.

If the document reloads, the DOM-ready callback is the correct place to register translations again for that document.

## In JavaScript

After the C++ registration call has executed:

```javascript
window.onTranslationsReady = function () {
  document.getElementById('title').textContent = window.t('$UI_TITLE');
};

// window.L10N is the raw key/value object.
console.log(window.L10N['$UI_TITLE']);
```

If translations are optional, guard them:

```javascript
const t = (key) => window.t?.(key) ?? key;
```

## Missing files

When no matching translation file is loaded, PrismaUI does not inject `window.L10N` or `window.t()`. The framework logs the expected `Data\Interface\Translations\...` path and the page should fall back to raw keys or its own defaults.

## Translation file format

One entry per line, key and value separated by a tab:

```text
$UI_TITLE	My Plugin
$UI_CLOSE	Close
$UI_SETTINGS	Settings
```
