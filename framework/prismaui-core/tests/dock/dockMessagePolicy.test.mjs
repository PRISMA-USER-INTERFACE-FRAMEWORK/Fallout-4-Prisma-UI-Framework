import { test } from "node:test";
import assert from "node:assert/strict";
import { isTrustedDockMessage } from "../../shell/src/dockMessagePolicy.js";

test("dock geometry accepts only the dock iframe at its resolved origin", () => {
    const dockWindow = {};
    const origin = "prisma://shell";
    assert.equal(isTrustedDockMessage({ source: dockWindow, origin }, dockWindow, origin), true);
});

test("dock geometry rejects a matching source from a different origin", () => {
    const dockWindow = {};
    assert.equal(
        isTrustedDockMessage({ source: dockWindow, origin: "https://example.test" }, dockWindow, "prisma://shell"),
        false,
    );
});

test("dock geometry rejects a matching origin from a different source", () => {
    assert.equal(
        isTrustedDockMessage({ source: {}, origin: "prisma://shell" }, {}, "prisma://shell"),
        false,
    );
});
