---
title: Run game-thread work from a button
sidebar_label: Game-thread work
sidebar_position: 7
---

# Run game-thread work from a button

Your page calls a button, and something in Fallout has to change. This recipe is the safe wiring for that, including the case where the call comes from your own thread instead of from the page.

## Two paths, one rule

All engine changes happen on the game thread. PrismaUI gives you two ways to get there from a page event:

- **`BindGameThreadUIEvent`** — the page calls a named function and the framework delivers your callback on its verified Fallout window thread.
- **`DispatchToGameThread`** — you have a callback (or your own thread) and queue work onto the same verified thread.

`BindUIEvent` listeners are also delivered on the game thread, which is fine for reading state and updating your own plugin. Use one of the two calls above when the action changes engine state and you want the guarantee to be explicit.

## Page side

```html
<button id="bless" type="button">Bless this weapon</button>
<p id="result"></p>
```

```js
document.getElementById('bless').addEventListener('click', function () {
  window.requestBlessing(JSON.stringify({ target: 'weapon', amount: 10 }));
});
```

## Plugin side — the page action

```cpp
struct BlessingRequest {
    std::string target;
    int         amount = 0;
};

static BlessingRequest g_request;
static PRISMA_UI_API::IVPrismaUI11* g_thread = nullptr;

// The framework guarantees this runs on the verified Fallout window thread.
static void OnBlessingRequested(const char* argument, void* userdata)
{
    auto* request = static_cast<BlessingRequest*>(userdata);
    REX::INFO("blessing {} by {}", request->target, request->amount);

    // Your engine work goes here. For example: find the player, change a weapon,
    // start a scene, apply a magic effect. Keep it short.
    ApplyWeaponBlessing(request->target, request->amount);

    // Tell the page. UI updates are safe from the game thread.
    g_api->InteropCall(g_view, "onBlessingDone", R"("done")");
}
```

Register it in `OnDomReady`:

```cpp
static void OnDomReady(PrismaView view)
{
    g_thread = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI11>();
    if (!g_thread) {
        // Flat provider only. VR has no V11.
        REX::WARN("game-thread dispatch is unavailable");
    }

    if (g_thread) {
        g_thread->BindGameThreadUIEvent(view, "requestBlessing", OnBlessingRequested, &g_request);
    } else {
        // Fallback: plain listener, still delivered on the game thread.
        g_api->BindUIEvent(view, "requestBlessing", [](const char* argument) {
            // parse into g_request and do the work
        });
    }
}
```

## Plugin side — work from your own thread

If something off-thread wants to change the game, queue it:

```cpp
static void ApplyFromWorkerThread()
{
    if (!g_thread) return;
    if (!g_thread->DispatchToGameThread([](void*) {
            ApplyWeaponBlessing("weapon", 10);
        }, nullptr)) {
        REX::WARN("game-thread dispatch refused the work");
    }
}
```

`DispatchToGameThread` returns `false` when dispatch is not available or the queue is full — and that `false` guarantees the callback will never run later. Treat it as a failed action, not as "probably fine".

Note that dispatch is always deferred: even if you are already on the game thread, the callback runs after the current menu/input stack unwinds. That is intentional — mutating the engine in the middle of another menu's callback is how you get odd state.

## `userdata` lifetime

`userdata` is yours. Keep it alive until the callback runs:

- a `static` struct, as above: fine;
- a heap pointer: you must know when it is safe to free it;
- do not free the payload inside the callback while the queue may still reference it.

When in doubt, copy the data into a static slot before dispatching. Small requests do not need heap allocation.

## What not to do

- Do not call engine write APIs from your own worker thread.
- Do not assume `BindUIEvent` is off-thread — it is not; but do not rely on that as your only safety when the work is engine-heavy.
- Do not spin-wait for the game thread from a page callback. Queue the work and return.
- Do not block for long inside a game-thread callback: a long engine operation there stalls the frame.

## Where to go next

- [API reference: BindGameThreadUIEvent](../api/BindGameThreadUIEvent) and [DispatchToGameThread](../api/DispatchToGameThread)
- [Current API extensions](../api-extensions) — which interfaces are flat-only
- [Talk between the page and the plugin](talk-to-your-plugin) — the messaging basics
- [When your code runs](../when-your-code-runs) — why the game thread matters
