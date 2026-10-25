# `Unfocus`

**Since:** `IVPrismaUI1`

```cpp
virtual void Unfocus(PrismaView view) noexcept = 0;
```

Removes focus from the view and restores game input. If `pauseGame` was `true` on the matching
`Focus` call, game time is restored as well.

Call `Hide` after `Unfocus` if you want the view invisible while not in use - `Unfocus` on its own
only releases input, it does not hide the view.

## See also

`Focus`, `Hide`.
