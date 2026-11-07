#!/usr/bin/env python3
"""Flat virtual A must be routed on the common input chains, not the native converter.

A used to be handled only inside ConvertButton, i.e. only when the native GFx conversion ran for the
press. Fallout's own conversion does not produce a usable Accept event for A, so A did nothing in
normal mode and worked only once native Gamepad routing took over. The route now lives in
RouteGamepadToView (which the menu and gameplay sinks already call), after native Gamepad, bound
controller actions and focus-entry handling, and it is remembered by the cross-chain gate.

The native converter is also its own chain identity. It used to report kMenu, the same bit the menu
sink uses: Start() clears a chain's bit, so the converter re-armed the event and the consumer saw it
twice.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src" / "PrismaUI"


def read(name: str) -> str:
    return (SRC / name).read_text(encoding="utf-8", errors="replace")


def body(source: str, signature: str) -> str:
    cursor = 0
    while True:
        start = source.find(signature, cursor)
        if start < 0:
            raise AssertionError(f"no definition found: {signature}")
        opening = source.find("{", start)
        semicolon = source.find(";", start)
        if opening != -1 and (semicolon == -1 or opening < semicolon):
            break
        cursor = start + len(signature)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError("unclosed body")


glyphs = read("ControllerGlyphs.cpp")
gate = read("ControllerEventGate.h")


def fail(message: str) -> None:
    print(f"FAIL: {message}")
    raise SystemExit(1)


route = body(glyphs, "bool RouteGamepadToView(")
native = route.index("WebRuntime::UsesNativeGamepad(focusedView)")
action = route.index("PrismaUI::ControllerActions::Handle(")
focus_entry = route.index("ControllerActions::HandleFocusEntry(")
eligibility = route.index("ControllerInputPolicy::VirtualPointerEligible(")
virtual_a = route.index("VirtualPointer::OnButton(")
if not native < action < focus_entry < eligibility < virtual_a:
    fail("virtual A must route after native Gamepad, actions, focus entry and the adapter gate")
if "g_nativeMenuReady.load()" not in route[eligibility:virtual_a]:
    fail("the virtual A gate must read the native adapter readiness")
if "consume(true)" not in route[virtual_a:]:
    fail("a consumed virtual A must be remembered by the cross-chain gate")
if glyphs.index("bool RouteGamepadToView(") < glyphs.index("std::atomic<bool> g_nativeMenuReady{false};"):
    fail("adapter readiness must be declared before the route that consults it")

native_branch = route[native:action]
if "VirtualPointerEligible" in native_branch or "VirtualPointer::OnButton" in native_branch:
    fail("native Gamepad must bypass the virtual-pointer adapter gate entirely")
if "consume(true)" not in native_branch:
    fail("native Gamepad must keep owning its own sequence")

converter = body(glyphs, "void ConvertButton(")
if "VirtualPointer::OnButton(" in converter:
    fail("ConvertButton must not special-case virtual A any more")
if "InputChain::kConverter" not in converter:
    fail("ConvertButton must route on its own kConverter chain identity")
if "InputChain::kMenu" in converter:
    fail("ConvertButton must not share the menu sink's chain identity")

menu_sink = body(glyphs, "class InputSink")
gameplay_sink = body(glyphs, "class GameplayInputSink")
if "InputChain::kMenu" not in menu_sink or "InputChain::kGameplay" not in gameplay_sink:
    fail("menu and gameplay sinks must keep their own chain identities")

if "kMenu = 1" not in gate or "kGameplay = 2" not in gate or "kConverter = 4" not in gate:
    fail("InputChain must keep kMenu/kGameplay/kConverter as distinct single bits")

send_menu = body(glyphs, "void SendMenuEvent(")
if "forwarded.Note(" not in send_menu:
    fail("SendMenuEvent must deduplicate one {key, pressed} per converter invocation")
if "MenuSender g_sendMenuEvent" not in glyphs:
    fail("the native menu sender detour is missing")

if "ControllerInputPolicy::ForwardGuard forwarded;" not in glyphs:
    fail("the per-invocation forward guard must live in ConversionContext")
if "std::exchange(g_conversion, {})" not in glyphs:
    fail("the forward guard must reset for every converter invocation")

forward_guard = read("ControllerForwardGuard.h")
if "kCapacity = 4" not in forward_guard:
    fail("the forward guard capacity is missing")
if "if (count_ >= entries_.size()) return false;" not in forward_guard:
    fail("a full forward guard must suppress an unrecorded pair instead of forwarding it")

print("controller virtual-A route guard passed")
