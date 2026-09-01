# `EnableActivateChoiceFilter`

**Since:** `IVPrismaUI7`

```cpp
virtual void EnableActivateChoiceFilter(bool enable, bool dropDefaultTake) noexcept = 0;
```

Enables or disables capture of the vanilla multi-activate choice row so V8 consumers can read the current choices with `GetActivateChoiceLabel` and trigger one with `TriggerActivateChoice`.

## Parameters

- `enable` - Enables or disables activate-choice capture.
- `dropDefaultTake` - Retained for ABI compatibility. **Ignored in PrismaUI_F4 2.1.0.** Default-Take filtering is not implemented.

## Important 2.1.0 behavior

This API should not be documented as a general-purpose row filter. Its supported purpose is to install the capture path required by the V8 read/trigger APIs.

Code written against older documentation that expected `dropDefaultTake=true` to remove the default Take row must not rely on that behavior.

## Example

```cpp
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI8>();
if (!api) return;

api->EnableActivateChoiceFilter(true, false);

char label[128]{};
if (api->GetActivateChoiceLabel(0, label, sizeof(label))) {
    logger::info("choice 0: {}", label);
}
```

## See also

- `GetActivateChoiceLabel`
- `TriggerActivateChoice`
- `SuppressActivateChoicePerk` - retained ABI placeholder; filtering is not implemented in 2.1.0
