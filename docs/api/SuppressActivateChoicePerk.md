# `SuppressActivateChoicePerk`

**Since:** `IVPrismaUI7`

```cpp
virtual void SuppressActivateChoicePerk(uint32_t perkFormID, bool suppress) noexcept = 0;
```

This method is retained as an **ABI-compatible placeholder** in PrismaUI_F4 2.1.0.

It does not currently filter perk-added activate choices. The framework logs the call and returns without changing the vanilla row.

## Parameters

- `perkFormID` - Retained ABI parameter.
- `suppress` - Retained ABI parameter.

## Do not depend on this for 2.1.0

Older documentation described this method as removing a perk's entry from the multi-activate row. That behavior is not implemented by the released 2.1.0 framework.

If your feature needs to inspect or activate the current row, use the V8 capture APIs instead:

- `EnableActivateChoiceFilter(true, false)`
- `GetActivateChoiceLabel`
- `TriggerActivateChoice`

## See also

`EnableActivateChoiceFilter`, `GetActivateChoiceLabel`, `TriggerActivateChoice`.
