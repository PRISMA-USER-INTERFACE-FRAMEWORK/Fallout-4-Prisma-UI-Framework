# `DispatchToGameThread`

**Since:** `IVPrismaUI11`

```cpp
virtual bool DispatchToGameThread(
    GameThreadTaskCallback callback,
    void* userdata
) noexcept = 0;
```

Queues native work onto PrismaUI's verified Fallout window thread.

## Returns

Returns `true` when the callback is accepted for deferred delivery. Returns `false` when verification is not ready, the dispatcher has failed closed, the queue is full, or `callback` is null. A `false` return guarantees that callback will not run later.

## Deferred execution

The callback is always deferred through the Fallout window message queue, even if the caller is already executing on that verified thread. This lets the current input/menu/detour stack unwind before engine state is mutated.

## `userdata` lifetime

`userdata` remains consumer-owned. After a successful call, keep it valid until the callback executes or until the owning attachment is torn down. Do not allocate a payload whose only deletion path is inside the callback, because queued work can be discarded during teardown.

## Example

```cpp
struct Work {
    std::uint32_t formId;
};

static Work g_work{};

static void Apply(void* userdata) {
    auto* work = static_cast<Work*>(userdata);
    // Touch Fallout-owned state here.
}

if (!api->DispatchToGameThread(Apply, &g_work)) {
    logger::warn("Game-thread dispatch unavailable");
}
```

## Provider boundary

V11 is exposed by the flat Fallout 4 provider. The VR provider does not advertise V11.

## See also

- [Current API extensions](../api-extensions.md)
- `IsGameThread`
- `BindGameThreadUIEvent`
