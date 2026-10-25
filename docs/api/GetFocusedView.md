# `GetFocusedView`

**Since:** `IVPrismaUI10`

```cpp
virtual PrismaView GetFocusedView() noexcept = 0;
```

Returns the view that currently holds framework focus, or `0` if none.

[`HasAnyActiveFocus`](HasAnyActiveFocus) (V1) answers the yes/no of "is anything focused";
`GetFocusedView` tells you **which** view it is.

## See also

- [`HasAnyActiveFocus`](HasAnyActiveFocus) - the yes/no version.
- [`HasFocus`](HasFocus) - test whether one specific view has focus.
- [`IsAnyPanelVisible`](IsAnyPanelVisible) - "is another interactive panel up", which already
  accounts for focus.

**Try it:** [Panel management](../panel-management).
