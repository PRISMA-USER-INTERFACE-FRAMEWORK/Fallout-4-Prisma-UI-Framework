---
title: Game-thread table
sidebar_label: Game-thread
sidebar_position: 32
---

# Game-thread table

Verified game-thread dispatch, thread checks and game-thread page events.

Discover GameThread when a page action has to touch Fallout state.

## Discover it

```cpp
GameThreadAPI gameThread{};
if (!Discover<ApiFeature::GameThread>(GameThreadApiVersion, gameThread)) {
    return;
}
```

## Methods

- [`DispatchToGameThread`](../api/DispatchToGameThread) - Queues native work onto PrismaUI's verified Fallout window thread.
- [`IsGameThread`](../api/IsGameThread)
- [`BindGameThreadUIEvent`](../api/BindGameThreadUIEvent) - Registers a JavaScript-to-native event whose native callback is delivered on PrismaUI's verified Fallout window thread.

## What to read with it

- [Run game-thread work from a button](../cookbook/run-game-thread-work)

## Related

- [Modern API](../modern-api)
- [API reference](../api-reference)
- [Cookbook](/docs/cookbook/)

