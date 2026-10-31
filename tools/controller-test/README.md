# PrismaControllerTest_F4

This standalone development mod isolates Prisma controller input. It uses its own Prisma view and does not modify Crafting or PrismaDebugger_F4.

The probe accepts only the two controller-provider runtimes enabled by PR736; every other runtime is rejected by `F4SEPlugin_Query`. Install the DLL under `F4SE/Plugins` and the three view files under `PrismaUI_F4/views/Interface/PrismaControllerTest_F4/`.

The Back open gesture uses the same `RE::BS_BUTTON_CODE` enum path as `framework/prismaui-core/src/PrismaUI/ControllerGlyphs.cpp:kGamepadButtons`. The runtime allowlist is the PR736 contract recorded in `docs/proof/0736-native-controller-re.md`.

## Input contract (normal mode vs native Gamepad mode)

| Input | Normal mode | Native Gamepad mode |
|---|---|---|
| Right stick | Moves the OS pointer; the compositor cursor follows it | Suspended; axes go to the page instead |
| A | Virtual left click at the pointer (`mousedown`/`mouseup`/`click`) | Gamepad button 0 only; it must not also synthesize a click |
| B, D-pad | Native converted browser keys (`keydown`/`keyup`: Escape/Arrow*) | Gamepad buttons/axes |
| X, Y, LB, RB, LT, RT, Start, LS, RS | Bound controller actions (`prisma-controller-action`) | Gamepad buttons/axes |
| A, B, D-pad while the OSK is open | Bound controller actions for the OSK (`probe.osk.*`), unbound when it closes | Gamepad buttons/axes |
| Page view | `window`/`document` events as above | `navigator.getGamepads()` path |

A is not a converted browser key. Live evidence in `evidence/20260915-flat-controller-api-diagnostic.md`
("the A/Accept button never reaches the webview") shows the vanilla engine emits no `converted menu=`
event at all for A (`code=0x11000`), unlike D-pad and B, so there is nothing for the native conversion
hook to forward; the browser's `"Unidentified"` keydowns do not come from a translated Accept key.
That is why A is delivered as a virtual left click instead.

## Test sequence

1. In gameplay, hold controller Back for 650 ms. The probe must open and focus.
2. Move the right stick. The pointer must move over the panel; the log must show the pointer records.
3. Press A once over a button. The page must show exactly one `mousedown`, one `mouseup` and one `click` for that target, and the log must show exactly one matching `request` line. See the acceptance counts below.
4. Press B and every D-pad direction. The page must show browser `keydown`/`keyup` (Escape and arrow keys). A must not produce a converted key.
5. Press X, Y, LB, RB, LT, RT, Start, LS, and RS. The page must show `action` records with press/release state and the log must show successful binds. Repeat records are expected only for mappings covered by the provider repeat policy.
6. Select `Release focus`, then press Back once. The log must show accepted and actual focus entry.
7. Toggle native Gamepad while focused. The page must show changing axes/buttons from `navigator.getGamepads()`; toggle it off before testing action/key delivery again. Right-stick pointer movement is suspended while native Gamepad is on, and A must not synthesize a click.
8. Exercise reload, destroy/recreate, focus churn, and close. The view must return with the same bindings and no stale handle.
9. On-screen keyboard: press A on `Keyboard: input`. The page must show `osk open #osk-input` and the log must show `keyboard bindings on` with six `action bind` lines (A, B, DUp, DDown, DLeft, DRight). Type with the D-pad and A; each key must log one `input` record from `osk-input` and no `action` record, because the OSK consumes them. LB/RB must change layout. B must restore the original value, show `osk closed`, and log `keyboard bindings off` with six `action unbind` lines, after which A clicks and B/D-pad are browser keys again. Repeat on `Keyboard: textarea` and `Keyboard: contenteditable`, including Paste and Done.
10. Hold a D-pad direction on the page, open the keyboard with a different path while still holding it, and confirm the held direction's repeats and release still reach the page and do not move the OSK selection.

### Acceptance counts

Normal mode, right stick over `Toggle native Gamepad`, one A press:

- exactly one `mousedown`, exactly one `mouseup`, exactly one `click`;
- exactly one `controller_toggle_native` request.

Native Gamepad mode, one A press:

- Gamepad button 0 only; no `mousedown`/`mouseup`/`click`, no `controller_toggle_native` request.

Useful log records include `PrismaUI API v12=acquired`, `modern controller API=true`, `action bind`, `focus entry bind Back`, `focus released`, `focus entry accepted`, `browser`, and `native gamepad`.

If the probe receives actions and native key events, the provider path is working and a Crafting failure is in its focus/consumer flow. If the probe also receives neither, that is a provider defect with a standalone reproduction.
