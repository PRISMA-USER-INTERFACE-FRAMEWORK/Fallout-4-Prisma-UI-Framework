# `GetActivateChoiceLabel`

**Since:** `IVPrismaUI8`

```cpp
virtual bool GetActivateChoiceLabel(uint32_t buttonIndex, char* outBuffer, size_t bufferSize) noexcept = 0;
```

Reads the label of the vanilla multi-activate choice currently captured at a given button slot -
for example `"Field Dress"` for a hunting-perk choice.

## Parameters

- `buttonIndex` - Slot index, `0`..`3`.
- `outBuffer` / `bufferSize` - Destination buffer. Up to `bufferSize - 1` bytes are copied, plus a
  null terminator.

## Requires

`EnableActivateChoiceFilter(true, ...)` (V7) to be active - capture only runs while that filter's
`PopulateData` detour is installed.

## Returns

`false` (buffer left untouched) if that slot has no valid captured choice right now, or the filter
isn't enabled.

## See also

`EnableActivateChoiceFilter` (V7), `TriggerActivateChoice`.
