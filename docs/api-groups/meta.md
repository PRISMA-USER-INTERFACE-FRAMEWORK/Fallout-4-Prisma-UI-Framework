---
title: Meta table
sidebar_label: Meta
sidebar_position: 33
---

# Meta table

Provider identity, API build information and capability discovery.

Discover Meta when you want to check what the installed provider can do before requesting other tables.

## Discover it

```cpp
MetaAPI meta{};
if (!Discover<ApiFeature::Meta>(MetaApiVersion, meta)) {
    return;
}
```

## Methods

- `ProviderName`
- `ApiBuild`
- `HasFeature`

## What to read with it

- [Modern API](../modern-api)

## Related

- [Modern API](../modern-api)
- [API reference](../api-reference)
- [Cookbook](/docs/cookbook/)

