# `GetFocusedView`

**Since:** `IVPrismaUI10`

```cpp
virtual PrismaView GetFocusedView() noexcept = 0;
```

Returns the view that currently holds framework focus, or `0` if none.

[`HasAnyActiveFocus`](HasAnyActiveFocus.md) (V1) answers the yes/no of "is anything focused";
`GetFocusedView` tells you **which** view it is.

## See also

- [`HasAnyActiveFocus`](HasAnyActiveFocus.md) - the yes/no version.
- [`HasFocus`](HasFocus.md) - test whether one specific view has focus.
- [`IsAnyPanelVisible`](IsAnyPanelVisible.md) - "is another interactive panel up", which already
  accounts for focus.
