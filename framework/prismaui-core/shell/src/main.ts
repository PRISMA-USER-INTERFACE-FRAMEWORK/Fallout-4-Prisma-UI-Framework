export {};

import { isTrustedDockMessage } from "./dockMessagePolicy.js";

// PrismaUI_F4 shell view-manager. The ONE top-level CEF page. Plugin views are dynamically-created
// sandboxed iframes managed here on behalf of the native framework: PrismaUI_F4_Host.dll drives this
// via window.__prismaShell.* (through CefFrame::ExecuteJavaScript), and this reports back via
// window.__prismaNative(channel, payload) (a native V8 binding injected only into trusted
// prisma:// frames by PrismaCefApp). See docs/CEF_MIGRATION_PLAN.md (A2/A8/A9).

declare global {
  interface Window {
    __prismaNative?: (channel: string, payload: string) => void;
    __prismaShell: PrismaShell;
  }
}

interface DockPayload {
  welcome: boolean;
  // Built as JSON by DockBridge and forwarded to the dock iframe
  // untouched, so this has to list every field the Host actually sends. `thumb` was missing here
  // for a while, which made the dock look like it could never read a mod's dock.json.
  views: { id: number; title: string; hidden: boolean; thumb: string }[];
}

interface PrismaShell {
  createView(id: number, url: string): void;
  destroyView(id: number): void;
  showView(id: number): void;
  hideView(id: number): void;
  setOrder(id: number, order: number): void;
  focusView(id: number): void;
  unfocusView(id: number): void;
  showBootAnimation(): void;
  showDock(payload: DockPayload): void;
  hideDock(): void;
}

const container = document.getElementById("views")!;
const views = new Map<number, HTMLIFrameElement>();

// Plugin views keep allow-same-origin so browser APIs such as localStorage and Cache work. They are
// no longer siblings under one prisma://views origin: ViewOrigin.h assigns each Interface/<Plugin>/
// folder a deterministic prisma://view-<hash>/ origin, and the scheme handler refuses to serve a
// different plugin folder under that host. Same-origin privileges therefore stay inside one plugin
// instead of exposing every Prisma consumer's storage/DOM to every other consumer.
//
// Site isolation remains disabled at Chromium process level because CEF OSR cannot composite OOPIF
// surfaces into the shared texture, but the browser's normal same-origin checks still apply.
const VIEW_SANDBOX = "allow-scripts allow-same-origin";

// dock.html/boot.html are served from prisma://shell/system/... -- the SAME host as their parent
// (prisma://shell/index.html), unlike plugin views (prisma://view-<hash>/...). Sandboxing a same-origin
// frame is either a no-op (allow-scripts + allow-same-origin -- a same-origin sandboxed frame can
// just strip its own sandbox attribute via script, confirmed 2026-07-11 by Chromium's own console
// warning "can escape its sandboxing") or actively harmful (allow-scripts alone forces the opaque
// per-load origin that broke FalloutChat's localStorage last time -- see the note above). These are
// first-party trusted shell content, not untrusted plugin/third-party HTML, so there's no isolation
// to buy either way: skip the sandbox attribute for them entirely instead of keeping a token
// combination that's either pointless or actively wrong.

function native(channel: string, payload: string): void {
  if (window.__prismaNative) window.__prismaNative(channel, payload);
}

// The dock iframe (framework system view). Its on-screen SIZE is driven by the dock itself via a
// "dockGeom" postMessage (see applyDockGeom): full-screen while the dock is expanded / the welcome
// card is up (so it captures clicks anywhere), but shrunk to just the top-right tab corner while
// collapsed -- so a focused plugin panel underneath receives every click outside that corner. The
// software cursor is drawn by the C++ compositor (decoupled from the DOM), so shrinking this frame
// never affects the cursor.
const dockFrame = document.createElement("iframe");
dockFrame.className = "prisma-system";
dockFrame.style.cssText = "position:absolute;top:0;left:0;width:100%;height:100%;border:none;background:transparent;display:none;z-index:90000;";
dockFrame.src = "./system/dock.html";
// Derive this from the browser's URL implementation at runtime. CEF decides the serialized origin
// for the registered prisma scheme, so hard-coding a string here would only be a guess.
const dockOrigin = new URL(dockFrame.src, window.location.href).origin;
let dockReady = false;
let pendingDock: DockPayload | null = null;
dockFrame.addEventListener("load", () => {
  dockReady = true;
  if (pendingDock) {
    dockFrame.contentWindow?.postMessage({ type: "setViews", payload: pendingDock }, "*");
    pendingDock = null;
  }
});
container.appendChild(dockFrame);

// Resize the dock iframe to match how much of the screen the dock currently needs to capture.
function applyDockGeom(mode: "full" | "tab"): void {
  const s = dockFrame.style;
  if (mode === "tab") {
    s.top = "0"; s.right = "0"; s.left = "auto"; s.bottom = "auto";
    s.width = "200px"; s.height = "200px";  // comfortably contains the 56px tab (22px inset) + badge/hover
  } else {
    s.top = "0"; s.left = "0"; s.right = "auto"; s.bottom = "auto";
    s.width = "100%"; s.height = "100%";
  }
}

// The first-party system dock reports its interactive footprint here.
window.addEventListener("message", (e) => {
  if (!isTrustedDockMessage(e, dockFrame.contentWindow, dockOrigin)) return;
  const d = e.data as { type?: string; mode?: string } | null;
  if (d && d.type === "dockGeom") applyDockGeom(d.mode === "tab" ? "tab" : "full");
});

// Visibility signal for the native compositor. The shell is the single source of truth for whether
// anything Prisma is on screen -- a view can be visible without being focused (always-on HUD widgets),
// so the C++ side cannot derive this from focus. Report only on change; the Host defaults to "visible"
// until told otherwise, so a missed initial report can never blank the UI, only cost an idle composite.
let bootActive = false;
let lastReportedVisible: boolean | null = null;

function computeShellVisible(): boolean {
  if (bootActive) return true;
  if (dockFrame.style.display !== "none") return true;
  for (const frame of views.values()) {
    if (frame.style.display !== "none") return true;
  }
  return false;
}

function reportVisibility(): void {
  const visible = computeShellVisible();
  if (visible === lastReportedVisible) return;
  lastReportedVisible = visible;
  native("shellVisible", visible ? "1" : "0");
}

const shell: PrismaShell = {
  createView(id, url) {
    if (views.has(id)) return;
    const iframe = document.createElement("iframe");
    iframe.className = "prisma-view";
    iframe.name = `prisma-view-${id}`;
    iframe.setAttribute("sandbox", VIEW_SANDBOX);
    iframe.addEventListener("load", () => {
      if (iframe.dataset.prismaDestroying === "1") return;
      native("domReady", String(id));
    });
    iframe.src = url;
    views.set(id, iframe);
    container.appendChild(iframe);
    reportVisibility();
  },
  destroyView(id) {
    const iframe = views.get(id);
    if (!iframe) return;

    // Logical destruction is immediate so native code can create a replacement without the old
    // view remaining interactive. Physical iframe removal is deferred one macrotask: removing a
    // live Chromium frame synchronously from inside the Host's ExecuteJavaScript turn can re-enter
    // frame teardown while the same shell turn is still creating the next view. PrismaCrafting hit
    // that as a second-CreateView stall before DOM ready. Deferring only the DOM removal lets the
    // current CEF stack unwind while keeping the destroyed view invisible and non-interactive now.
    views.delete(id);
    iframe.dataset.prismaDestroying = "1";
    iframe.style.pointerEvents = "none";
    iframe.style.display = "none";
    setTimeout(() => iframe.remove(), 0);
    reportVisibility();
  },
  showView(id) {
    const iframe = views.get(id);
    if (iframe) iframe.style.display = "";
    reportVisibility();
  },
  hideView(id) {
    const iframe = views.get(id);
    if (iframe) iframe.style.display = "none";
    reportVisibility();
  },
  setOrder(id, order) {
    const iframe = views.get(id);
    if (iframe) iframe.style.zIndex = String(order);
  },
  focusView(id) {
    // Bail on an unknown id rather than running the loop below: that would set EVERY view to
    // pointer-events:none and leave no view able to receive clicks, with no path back.
    const iframe = views.get(id);
    if (!iframe) return;
    // Only the newly-focused view should be able to receive clicks -- every other view (including
    // always-shown passive HUD widgets that never focus at all) stays pointer-events:none from the
    // base CSS rule, so it can't swallow input meant for whatever's now on top / underneath it.
    for (const [otherId, frame] of views) {
      frame.style.pointerEvents = otherId === id ? "auto" : "none";
    }
    // The dock is not in `views`, so it never got the treatment above. At z-index 90000 with a
    // full-screen "full" footprint it is a transparent click sink over the view being focused.
    applyDockGeom("tab");
    iframe.focus();
  },
  unfocusView(id) {
    const iframe = views.get(id);
    if (iframe) iframe.style.pointerEvents = "none";
  },
  showBootAnimation() {
    const boot = document.createElement("iframe");
    boot.className = "prisma-system";
    boot.style.cssText = "position:absolute;top:24px;right:24px;width:340px;height:96px;border:none;background:transparent;z-index:100000;";
    boot.src = "./system/boot.html";
    container.appendChild(boot);
    bootActive = true;
    reportVisibility();
    setTimeout(() => {
      boot.remove();
      bootActive = false;
      reportVisibility();
    }, 7000);
  },
  showDock(payload) {
    dockFrame.style.display = "";
    // Default to the tab footprint so an already-open panel underneath isn't blocked; the dock posts
    // "full" itself if it needs the whole screen (welcome / expanded).
    applyDockGeom("tab");
    if (dockReady) {
      dockFrame.contentWindow?.postMessage({ type: "setViews", payload }, "*");
    } else {
      pendingDock = payload;
    }
    reportVisibility();
  },
  hideDock() {
    dockFrame.style.display = "none";
    reportVisibility();
  },
};

window.__prismaShell = shell;

// Push the initial state (nothing shown yet) so the Host can drop idle composites from the first frame.
reportVisibility();

console.log("[PrismaShell] view-manager ready");