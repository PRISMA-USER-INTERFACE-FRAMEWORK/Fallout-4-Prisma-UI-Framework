---
title: Localization table
sidebar_label: Localization
sidebar_position: 36
---

# Localization table

Legacy Fallout-string translations plus V4 JSON localization.

Discover Localization when the page reads translated strings from files.

## Discover it

```cpp
LocalizationAPI localization{};
if (!Discover<ApiFeature::Localization>(LocalizationApiVersion, localization)) {
    return;
}
```

## Methods

- [`RegisterTranslations`](../api/RegisterTranslations) - Loads the current Fallout 4 translation file for pluginName, builds the window.L10N / window.t() helper script, and injects that script into the current live document through...
- `RegisterTranslationsV4`

## What to read with it

- [Translate your interface](../cookbook/translate-your-ui)

## Related

- [Modern API](../modern-api)
- [API reference](../api-reference)
- [Cookbook](/docs/cookbook/)

