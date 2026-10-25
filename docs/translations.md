---
title: 'Translations'
---

:::tip New to PrismaUI?
Start with [Your first panel](quick-start) for a complete walkthrough, or copy a working example from the [Cookbook](/docs/cookbook/).
:::

# Translations

This makes your interface follow the player's game language. You ship a JSON file per language, and PrismaUI hands the matching strings to the page as `window.PrismaL10N`.

There are two translation APIs. **V4 JSON** is the one to use for new mods. The old Fallout-string `.txt` path still works for existing integrations; it is documented at the bottom.

## The short version

1. Put `MyPlugin_en.json` in `Data/Interface/Translations/`.
2. In `OnDomReady`, call `RegisterTranslationsV4(view, "MyPlugin")`.
3. In the page, use `window.PrismaL10N.t('KEY')`.

The rest of this page is the detail.

## The files

```text
Data/Interface/Translations/
├── MyPlugin_en.json
├── MyPlugin_de.json
└── MyPlugin_fr.json
```

`MyPlugin_en.json`:

```json
{
  "MENU_TITLE": "My Panel",
  "MENU_CLOSE": "Close",
  "ITEMS_COUNT": "Items: {{count}}"
}
```

`MyPlugin_de.json`:

```json
{
  "MENU_TITLE": "Mein Panel",
  "MENU_CLOSE": "Schließen",
  "ITEMS_COUNT": "Gegenstände: {{count}}"
}
```

The file name is `<pluginName>_<language>.json`. The plugin name is the string you pass to the API; the language is the code Fallout reports (`en`, `de`, `fr`, `it`, `es`, `esmx`, `cn`, `ja`, `pl`, `ru`, `ptbr`, …).

Add English last if you like — it is the fallback for every other language.

## Registering from the plugin

```cpp
#include "PrismaUI_F4_Modern_API.h"
using namespace PRISMA_UI_FLAT_API;

static LocalizationAPI g_localization{};

// once, at kGameDataReady
if (!Discover<ApiFeature::Localization>(LocalizationApiVersion, g_localization)) {
    REX::WARN("this PrismaUI build has no V4 localization");
}

static void OnDomReady(PrismaView view)
{
    if (g_localization.RegisterTranslationsV4 &&
        !g_localization.RegisterTranslationsV4(view, "MyPlugin")) {
        REX::WARN("no usable translation JSON for MyPlugin");
    }

    // Only now ask the page to paint translated text.
    g_api->Invoke(view, "window.applyTranslations()");
}
```

`RegisterTranslationsV4` returns `false` when nothing usable was loaded. Then `window.PrismaL10N` is simply not defined, and the page keeps its fallback text.

If the page reloads, the DOM is new: register again from `OnDomReady`.

## Using the strings in the page

```js
window.applyTranslations = function () {
  const l10n = window.PrismaL10N;

  // Fall back to the key if translations never loaded.
  const t = l10n ? l10n.t : function (key) { return key; };

  document.getElementById('title').textContent = t('MENU_TITLE');
  document.getElementById('close').textContent = t('MENU_CLOSE');

  // {{name}} placeholders are replaced from the second argument.
  document.getElementById('count').textContent = t('ITEMS_COUNT', { count: 12 });
};
```

`window.PrismaL10N` contains:

| Property | Meaning |
| --- | --- |
| `locale` | The language PrismaUI detected, e.g. `"de"`. |
| `messages` | The flat key/value object, e.g. `PrismaL10N.messages['MENU_TITLE']`. |
| `t(key, vars)` | Returns the string, replaces `{{name}}` from `vars`, returns the key if missing. |

Write the HTML so it makes sense before translations arrive — for example, put English text directly in the page. If no translation file is found, the player still gets a usable interface.

## Which file wins

PrismaUI builds the list from the detected language:

1. English (`en`) is always loaded first as a base.
2. For a regional language like `de-DE`, the base language (`de`) is loaded next, if it differs.
3. The full locale (`de-DE`) is loaded last.

Later files win, so a missing key in a language file falls back to your English text. That is why keeping the English file complete matters even if you ship one language.

Nested JSON objects are flattened into dotted keys, so `{"menu":{"title":"My Panel"}}` is also reachable as `menu.title`.

## Loose files and BA2

The loader resolves `Interface/Translations/<pluginName>_<language>.json` through Fallout's resource system. A loose file and a file inside a BA2 use the same path — you do not need a second code path, and you do not need to load the JSON from the page.

## Testing translations

1. Copy the files to `Data/Interface/Translations/`.
2. Start the game with a non-English language selected.
3. Open the page: the strings should be translated.
4. Delete the non-English file, restart, and confirm English is used instead.
5. Add a key to only one file and confirm the other language falls back for that key.

## The legacy `.txt` API

Old mods use `RegisterTranslations(view, "MyPlugin_F4")` and this file:

```text
Data/Interface/Translations/MyPlugin_F4_en.txt
```

Format: UTF-16 LE with BOM (UTF-8 also works), one `$KEY<TAB>value` per line:

```text
$CLOSE	Close
$MENU_TITLE	My Panel
$ITEM_COUNT	Items: {0}
```

The page then gets `window.L10N` and `window.t()`:

```js
window.onTranslationsReady = function () {
  document.getElementById('title').textContent = window.t('$MENU_TITLE');
};
```

Call it from `OnDomReady` and invoke the page initializer afterwards, exactly like V4. The legacy path injects into the current live document; it is not a pre-document hook. Do not mix V4 JSON and the legacy `.txt` for one view.

## Common mistakes

- **File name does not match the API name.** `RegisterTranslationsV4(view, "MyPlugin")` needs `MyPlugin_en.json`, not `MyPlugin_F4_en.json`.
- **Called before the view exists.** Register in `OnDomReady`.
- **Used in a script that runs before registration.** The strings are injected at registration; page module-level code runs earlier.
- **Only English file shipped and the game is German.** That works (English fallback) — but the file still has to exist.
- **`innerHTML` with translated text.** Use `textContent`. Treat translation values as data.

## Where to go next

- [Translate your interface](cookbook/translate-your-ui) — complete recipe
- [`RegisterTranslations`](api/RegisterTranslations) — the numbered API version
- [Modern API](modern-api) — feature discovery in general
- [The HTML side](html-views) — using strings safely in the page
