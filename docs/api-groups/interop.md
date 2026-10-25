---
title: Interop table
sidebar_label: Interop
sidebar_position: 35
---

# Interop table

C++ to page and page to C++ messaging: invoke, interop calls, JS listeners, UI events and console callbacks.

Discover Interop for any plugin with a real page. It is the bridge half of the framework.

## Discover it

```cpp
InteropAPI interop{};
if (!Discover<ApiFeature::Interop>(InteropApiVersion, interop)) {
    return;
}
```

## Methods

- [`Invoke`](../api/Invoke) - Evaluates an arbitrary JavaScript expression in the view's context.
- [`InteropCall`](../api/InteropCall) - Calls a named JavaScript function via the JS Interop API.
- [`RegisterJSListener`](../api/RegisterJSListener) - Exposes a C++ callback to JavaScript.
- [`BindUIEvent`](../api/BindUIEvent) - Same shape as RegisterJSListener: exposes a C++ callback to JavaScript as window.functionName(arg).
- [`RegisterConsoleCallback`](../api/RegisterConsoleCallback) - Receives every console.log/warn/error/debug/info call from the view's JS context.

## What to read with it

- [Talk between the page and the plugin](../cookbook/talk-to-your-plugin)

## Related

- [Modern API](../modern-api)
- [API reference](../api-reference)
- [Cookbook](/docs/cookbook/)

