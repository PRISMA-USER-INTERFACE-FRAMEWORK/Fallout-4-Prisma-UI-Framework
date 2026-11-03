---
title: Translate your interface
sidebar_label: Translate your UI
sidebar_position: 6
---

# Translate your interface

This makes the panel follow the player's game language. You ship one JSON file per language, and PrismaUI injects the matching strings into the page as `window.PrismaL10N`.

The JSON path is the V4 localization API. It works with loose files and with files packed in a BA2.

## 1. Add the translation files

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
  "MENU_SCAN": "Scan area",
  "MENU_ITEMS": "Items: {{count}}",
  "MENU_CLOSE": "Close"
}
```

`MyPlugin_de.json`:

```json
{
  "MENU_TITLE": "Mein Panel",
  "MENU_SCAN": "Bereich scannen",
  "MENU_ITEMS": "Gegenstände: {{count}}",
  "MENU_CLOSE": "Schließen"
}
```

The file name matters: `<pluginName>_<language>.json`, where the plugin name is the one you pass to the API and the language matches Fallout's selected language (`en`, `de`, `fr`, `it`, `es`, `esmx`, `cn`, `ja`, `pl`, `ru`, `ptbr`, …). English is the fallback when the player's language file is missing.

Keep the encoding UTF-8 (with or without BOM). Nested keys also work; they are flattened to dotted names.

## 2. Register the translations from the plugin

Register in `OnDomReady`, before the page starts using strings:

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
    if (g_localization.RegisterTranslationsV4) {
        if (!g_localization.RegisterTranslationsV4(view, "MyPlugin")) {
            REX::WARN("no usable translation JSON for MyPlugin");
        }
    }

    // Now it is safe to ask the page to paint text.
    g_api->Invoke(view, "window.applyTranslations()");
}
```

`RegisterTranslationsV4` returns `false` when nothing usable was loaded — usually a missing file or malformed JSON. The page then keeps its fallback text.

## 3. Use the strings in the page

`window.PrismaL10N` has three parts:

- `locale` — the language PrismaUI detected, for example `"de"`.
- `messages` — the flat key/value object.
- `t(key, vars)` — returns the string, replaces `{{name}}` placeholders, and returns the key itself when it is missing.

```js
window.applyTranslations = function () {
  const l10n = window.PrismaL10N;
  const t = l10n ? l10n.t : function (key) { return key; };

  document.getElementById('title').textContent = t('MENU_TITLE');
  document.getElementById('scan').textContent = t('MENU_SCAN');
  document.getElementById('close').textContent = t('MENU_CLOSE');

  // Interpolation: {{count}} is replaced by the second argument.
  document.getElementById('items').textContent = t('MENU_ITEMS', { count: 12 });
};
```

Write the page so it is readable before translations arrive — for example, put the English strings directly in the HTML. If translations never load, the player still sees a usable page.

## 4. Test it

1. Copy the JSON files to `Data/Interface/Translations/` (or pack them into `Interface/Translations/` in a BA2).
2. Start the game with the German language selected.
3. Open the panel. The title and buttons should be German, and `Items: 12` should become `Gegenstände: 12`.
4. Delete `MyPlugin_de.json`, restart, and confirm the English file is used instead.

## Notes

- **Register before you paint.** `RegisterTranslationsV4` injects the strings into the live document. Calling it from `OnDomReady` and then telling the page to render is the reliable order.
- **Do not preload them in a script tag.** The JSON is read by the framework, not fetched by the page.
- **English is the fallback.** If the player's language file exists but a key is missing, the key falls back to the key name, not to the English file. Keep every language file complete, or generate it from the English one.
- **BA2 works.** The same path inside a packed archive is read through Fallout's resource system — no separate loading code.
- **Legacy format.** Older mods use `RegisterTranslations` with a `<PluginName>_<lang>.txt` file in the same folder. The two APIs are separate; do not mix them for one view.
- **Do not build HTML from translations with `innerHTML`.** Treat them as text. `textContent` is the safe default.

## Where to go next

- [Translations](../translations) for the full contract and the legacy format
- [The HTML side](../html-views) for the rest of the page setup
- [API reference: RegisterTranslations](../api/RegisterTranslations) for the numbered-interface version
