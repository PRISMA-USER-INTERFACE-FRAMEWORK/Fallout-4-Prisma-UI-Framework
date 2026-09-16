---
title: 'Translations'
---
# Translations

PrismaUI F4 supports Fallout 4-style translation files and exposes loaded strings to a view through `window.L10N` and `window.t()`.

In **PrismaUI_F4 2.1.0**, `RegisterTranslations` injects those helpers into the **current live document through `Invoke`**. It is not a pre-document injection hook, so call it from the view's DOM-ready callback before application code that requires translations.

---

## File format

Translation files use the Fallout 4 UI-string format:

- **Encoding:** UTF-16 LE with BOM (`FF FE`) is the normal release format.
- UTF-8 is also accepted by PrismaUI for development convenience.
- **One entry per line:** `$KEY<tab>Translated string`
- **Keys start with `$`**
- Lines that do not start with `$` are ignored.

```text
$CLOSE	Close
$MY_MENU_TITLE	My Menu
$ITEM_COUNT	Items: {0}
```

---

## File location

```text
Data\Interface\Translations\<PluginName>_<lang>.txt
```

| Part | Example |
|---|---|
| `PluginName` | The base name passed to `RegisterTranslations`, for example `MyPlugin_F4` |
| `lang` | Lowercase detected language code |

Example:

```text
Data\Interface\Translations\MyPlugin_F4_en.txt
```

PrismaUI first tries `<PluginName>_<lang>.txt`. For a non-English language, it falls back to `<PluginName>_en.txt` when the requested language file is missing.

---

## Language codes

Common supported codes include:

| Code | Language |
|---|---|
| `en` | English |
| `de` | German |
| `fr` | French |
| `it` | Italian |
| `es` | Spanish |
| `esmx` | Spanish (Mexico) |
| `cn` | Chinese (Simplified) |
| `ja` | Japanese |
| `pl` | Polish |
| `ru` | Russian |
| `ptbr` | Portuguese (Brazil) |

---

## C++ setup

Register translations from DOM-ready, then run whatever page initializer consumes them:

```cpp
static PRISMA_UI_API::IVPrismaUI10* g_api = nullptr;
static PrismaView g_view = 0;

static void OnDomReady(PrismaView view)
{
    g_api->RegisterTranslations(view, "MyPlugin_F4");
    g_api->Invoke(view, "window.onTranslationsReady && window.onTranslationsReady()");
}

static void CreateMenu()
{
    g_view = g_api->CreateView("MyPlugin/menu.html", OnDomReady);
    if (g_view) {
        g_api->Hide(g_view);
    }
}
```

This ordering matters. In the released 2.1.0 implementation, `RegisterTranslations` parses the translation file, builds a JavaScript helper script, and sends that script to the view through the normal `Invoke` path.

Do **not** rely on `window.t` from module-level code that runs before DOM-ready.

If the page reloads and the view's DOM-ready callback fires again, register translations again for that document.

---

## JavaScript usage

Prepare an initializer that C++ can call after `RegisterTranslations`:

```javascript
window.onTranslationsReady = function () {
  const t = (key) => window.t?.(key) ?? key;

  document.getElementById('close-btn').textContent = t('$CLOSE');
  document.getElementById('title').textContent = t('$MY_MENU_TITLE');
};
```

After successful registration:

```javascript
window.t('$CLOSE');
window.L10N['$CLOSE'];
```

If no translation file was loaded, PrismaUI leaves those globals undefined. Always use a fallback when translations are optional:

```javascript
const t = (key) => window.t?.(key) ?? key;
```

---

## Example mod layout

```text
mods/MyPlugin_F4/
├── F4SE/Plugins/
│   └── MyPlugin_F4.dll
├── PrismaUI_F4/
│   └── views/
│       └── MyPlugin/
│           └── menu.html
└── Interface/
    └── Translations/
        ├── MyPlugin_F4_en.txt
        ├── MyPlugin_F4_de.txt
        └── MyPlugin_F4_fr.txt
```

---

## Notes

- Keys are case-sensitive.
- If no requested-language or English fallback file exists, `window.L10N` / `window.t` are not injected.
- Values can contain HTML-like characters. Escape untrusted or user-controlled values before inserting them into `innerHTML`.
- Use the DOM-ready callback as the synchronization point between native translation registration and JavaScript that consumes translations.
