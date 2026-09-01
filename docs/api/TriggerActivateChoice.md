# `TriggerActivateChoice`

**Since:** `IVPrismaUI8`

```cpp
virtual bool TriggerActivateChoice(uint32_t buttonIndex) noexcept = 0;
```

Fires the captured choice at a given button slot, replaying the same call the engine makes when the
vanilla row's corresponding button is actually clicked.

## Parameters

- `buttonIndex` - Slot index, `0`..`3`, matching `GetActivateChoiceLabel`.

## Call timing

Call this soon after reading the label with `GetActivateChoiceLabel`. The captured listener is not
reference-counted here, so it can go stale if the player's target changes in between the two calls.

## Returns

`false` if that slot is empty.

## See also

`GetActivateChoiceLabel`, `EnableActivateChoiceFilter` (V7).
