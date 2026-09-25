import { mkdir, readdir, stat, writeFile } from "node:fs/promises";
import { dirname, relative, resolve } from "node:path";
import { fetchRawFile, listPathsUnder } from "../github.js";
import { getHeader, getModernHeader, getVrHeader } from "./getHeader.js";

const EXAMPLE_PREFIX = "example-f4se-plugin/";
const EXAMPLE_TOKEN = "PrismaUI-F4-Example";
const LEGACY_HEADER_PATH = "src/PrismaUI_F4_API.h";
const MODERN_HEADER_PATH = "src/PrismaUI_F4_Modern_API.h";
const VR_HEADER_PATH = "src/PrismaUI_F4VR_API.h";
const PLUGIN_NAME_PATTERN = /^[A-Za-z][A-Za-z0-9_-]*$/;

export type ScaffoldApiStyle = "modern" | "legacy";

export interface ScaffoldResult {
  targetPath: string;
  filesWritten: string[];
  apiStyle: ScaffoldApiStyle;
}

const MODERN_MAIN = `#include "PCH.h"
#include "PrismaUI_F4_Modern_API.h"
#include "keyhandler/keyhandler.h"

using namespace PRISMA_UI_FLAT_API;

static ViewAPI g_viewApi{};
static InteropAPI g_interop{};
static ControllerAPI g_controller{};
static PrismaView g_view = 0;
static bool g_visible = false;

static void ClosePanel()
{
    if (!g_view || !g_viewApi.IsValid(g_view)) return;
    g_visible = false;
    g_viewApi.Unfocus(g_view);
    g_viewApi.Hide(g_view);
}

static void OnDomReady(PrismaView view)
{
    g_interop.RegisterConsoleCallback(view, [](PrismaView, ConsoleMessageLevel level, const char* message) {
        switch (level) {
        case ConsoleMessageLevel::Error:
            REX::CRITICAL("[JS] {}", message ? message : "");
            break;
        case ConsoleMessageLevel::Warning:
            REX::WARN("[JS] {}", message ? message : "");
            break;
        default:
            REX::INFO("[JS] {}", message ? message : "");
            break;
        }
    });

    g_interop.RegisterJSListener(view, "requestClose", [](const char*) { ClosePanel(); });
    g_interop.RegisterJSListener(view, "sendDataToF4SE", [](const char* data) {
        REX::INFO("JS->C++: {}", data ? data : "");
    });

    g_controller.BindControllerAction(view, "B", "panel.close");
    g_interop.Invoke(view, "window.init && window.init()", nullptr);
}

static void CreateView()
{
    if (g_view && g_viewApi.IsValid(g_view)) return;
    g_view = g_viewApi.CreateView("${EXAMPLE_TOKEN}/index.html", OnDomReady);
    if (!g_view) {
        REX::CRITICAL("PrismaUI_F4 CreateView failed");
        return;
    }
    g_controller.SetViewRole(g_view, ViewRole::kPanel);
    g_viewApi.Hide(g_view);
}

static void Toggle()
{
    if (!g_view || !g_viewApi.IsValid(g_view)) return;
    g_visible = !g_visible;
    if (g_visible) {
        g_viewApi.Show(g_view);
        g_viewApi.Focus(g_view, false, false);
        g_interop.Invoke(g_view, "window.updateFocusLabel && window.updateFocusLabel('Focused. Press F3 to close.')", nullptr);
    } else {
        ClosePanel();
    }
}

static void OnMessage(F4SE::MessagingInterface::Message* message)
{
    if (!message) return;

    if (message->type == F4SE::MessagingInterface::kGameDataReady) {
        const bool ready =
            Discover<ApiFeature::View>(ViewApiVersion, g_viewApi) &&
            Discover<ApiFeature::Interop>(InteropApiVersion, g_interop) &&
            Discover<ApiFeature::Controller>(ControllerApiVersion, g_controller);
        if (!ready) {
            REX::CRITICAL("Required PrismaUI_F4 modern feature tables are unavailable");
            return;
        }
        KeyHandler::RegisterSink();
        [[maybe_unused]] auto registered = KeyHandler::GetSingleton()->Register(
            static_cast<uint32_t>(RE::BS_BUTTON_CODE::kF3), KeyEventType::KEY_DOWN, Toggle);
        return;
    }

    if ((message->type == F4SE::MessagingInterface::kPostLoadGame ||
         message->type == F4SE::MessagingInterface::kNewGame) &&
        g_viewApi.CreateView) {
        CreateView();
    }
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* intfc)
{
    F4SE::Init(intfc);
    F4SE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
`;

const MODERN_README = [
  "# " + EXAMPLE_TOKEN,
  "",
  "This scaffold uses the preferred PrismaUI_F4 2.2.0 modern feature-table API.",
  "",
  "It discovers View, Interop, and Controller independently through PrismaUI_F4_Modern_API.h. Add other tables only when needed:",
  "",
  "- Localization for V4 JSON translations",
  "- GameThread for verified Fallout-thread dispatch",
  "- Render for offscreen and geometry binding",
  "- Input for selective input regions",
  "- Menu for vanilla HUD/menu integration",
  "- Meta for capability discovery",
  "",
  "The stable V1-V12 compatibility header is also included for code that still needs numbered interfaces.",
  "",
  "Place web assets under Data/PrismaUI_F4/views/" + EXAMPLE_TOKEN + "/ and deploy the DLL under Data/F4SE/Plugins/.",
  "",
  "See the repository Modern API, Controller Actions, Translations, and Getting Started guides for the released contracts.",
  "",
].join("\n");

async function assertWritableTarget(absTarget: string, overwrite: boolean): Promise<void> {
  let stats;
  try {
    stats = await stat(absTarget);
  } catch {
    return;
  }
  if (!stats.isDirectory()) throw new Error('"' + absTarget + '" already exists and is not a directory.');
  if (overwrite) return;
  if ((await readdir(absTarget)).length > 0) {
    throw new Error('"' + absTarget + '" already exists and is not empty. Pass overwrite=true to write into it.');
  }
}

export async function scaffoldPlugin(
  pluginName: string,
  targetPath: string,
  overwrite = false,
  apiStyle: ScaffoldApiStyle = "modern"
): Promise<ScaffoldResult> {
  if (!PLUGIN_NAME_PATTERN.test(pluginName)) {
    throw new Error(
      'Invalid plugin name "' + pluginName + '". Use letters, digits, hyphens, and underscores, starting with a letter.'
    );
  }

  const absTarget = resolve(targetPath);
  await assertWritableTarget(absTarget, overwrite);

  const sourcePaths = await listPathsUnder(EXAMPLE_PREFIX);
  if (sourcePaths.length === 0) throw new Error('Found no files under "' + EXAMPLE_PREFIX + '".');

  const [legacyHeader, modernHeader, vrHeader] = await Promise.all([getHeader(), getModernHeader(), getVrHeader()]);
  const files = await Promise.all(
    sourcePaths.map(async (sourcePath) => {
      const relativePath = sourcePath.slice(EXAMPLE_PREFIX.length);
      const destPath = resolve(absTarget, relativePath);
      if (relative(absTarget, destPath).startsWith("..")) {
        throw new Error("Refusing to write outside the target directory: " + relativePath);
      }

      let rawContent: string;
      if (relativePath === LEGACY_HEADER_PATH) rawContent = legacyHeader;
      else if (relativePath === VR_HEADER_PATH) rawContent = vrHeader;
      else if (apiStyle === "modern" && relativePath === "src/main.cpp") rawContent = MODERN_MAIN;
      else if (apiStyle === "modern" && relativePath === "README.md") rawContent = MODERN_README;
      else rawContent = await fetchRawFile(sourcePath);

      return {
        relativePath,
        destPath,
        content: rawContent.split(EXAMPLE_TOKEN).join(pluginName),
      };
    })
  );

  if (apiStyle === "modern") {
    files.push({
      relativePath: MODERN_HEADER_PATH,
      destPath: resolve(absTarget, MODERN_HEADER_PATH),
      content: modernHeader,
    });

    const scriptPath = resolve(absTarget, "view", "script.js");
    const scriptIndex = files.findIndex((file) => file.destPath === scriptPath);
    if (scriptIndex >= 0) {
      files[scriptIndex] = {
        ...files[scriptIndex],
        content:
          files[scriptIndex].content +
          "\nwindow.addEventListener('prisma-controller-action', function(event) {\n" +
          "    if (event.detail && event.detail.action === 'panel.close' && event.detail.state === 'pressed') closePanel();\n" +
          "});\n",
      };
    }
  }

  const filesWritten: string[] = [];
  for (const file of files) {
    await mkdir(dirname(file.destPath), { recursive: true });
    await writeFile(file.destPath, file.content, "utf8");
    filesWritten.push(file.relativePath);
  }

  return { targetPath: absTarget, filesWritten, apiStyle };
}
