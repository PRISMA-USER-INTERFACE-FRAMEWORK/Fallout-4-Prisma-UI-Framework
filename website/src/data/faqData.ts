// FAQ entries for the site-wide FaqWidget. Answers are plain text (short, no markdown
// rendering) with an optional link to the doc page that covers the topic in full.
export type FaqEntry = {
  id: string;
  question: string;
  answer: string;
  keywords: string[];
  link?: string;
  linkLabel?: string;
};

export const FAQ_DATA: FaqEntry[] = [
  {
    id: 'what-is-prismaui',
    question: 'What is PrismaUI F4?',
    answer:
      'A native F4SE framework that lets Fallout 4 mods build UI with HTML, CSS, and JavaScript instead of editing Scaleform SWFs. PrismaUI_F4 2.1.0 uses Ultralight 1.4.0 in-process; the old legacy-runtime host and subprocess are retired.',
    keywords: ['what', 'prismaui', 'framework', 'about', 'overview', 'ultralight'],
    link: '/docs/what-is-prismaui',
    linkLabel: 'What is PrismaUI F4?',
  },
  {
    id: 'scaleform-editing',
    question: 'Does PrismaUI edit or replace Scaleform menus?',
    answer:
      'No. PrismaUI does not require editing SWF files. It renders a separate UI layer and can suppress selected vanilla HUD widgets and menus at runtime when a mod wants to replace them.',
    keywords: ['scaleform', 'swf', 'actionscript', 'replace', 'edit', 'vanilla menu'],
    link: '/docs/vanilla-ui-suppression',
    linkLabel: 'Vanilla UI suppression',
  },
  {
    id: 'actionscript-needed',
    question: 'Do I need to know ActionScript or Scaleform?',
    answer:
      'No. A PrismaUI consumer is ordinary HTML, CSS, and JavaScript controlled by an F4SE plugin through the Prisma C++ API.',
    keywords: ['actionscript', 'scaleform', 'learn', 'need to know', 'requirements'],
    link: '/docs/what-is-prismaui',
    linkLabel: 'What is PrismaUI F4?',
  },
  {
    id: 'where-to-start',
    question: 'Where do I start?',
    answer:
      'New to F4SE plugin development: start with Getting Started. Already have a working F4SE plugin: use Quick Start and copy the current PrismaUI_F4_API.h into your project.',
    keywords: ['start', 'begin', 'tutorial', 'setup', 'new'],
    link: '/docs/getting-started',
    linkLabel: 'Getting Started',
  },
  {
    id: 'example-plugin',
    question: 'Is there a working example plugin?',
    answer:
      'Yes. The repository includes a complete F4SE example plugin covering view creation, the C++/JS bridge, Papyrus integration, logging, and common lifecycle patterns.',
    keywords: ['example', 'sample', 'demo', 'plugin', 'reference'],
    link: '/docs/what-is-prismaui',
    linkLabel: 'What is PrismaUI F4?',
  },
  {
    id: 'web-frameworks',
    question: 'What web frameworks can I use?',
    answer:
      'Vanilla JS, React, Vue, Svelte, and similar stacks can be used when they build to static assets compatible with Ultralight 1.4.0. Self-host the required JS, CSS, fonts, and images with your mod.',
    keywords: ['react', 'vue', 'svelte', 'framework', 'javascript', 'web stack'],
    link: '/docs/modern-frameworks',
    linkLabel: 'Modern frameworks',
  },
  {
    id: 'nothing-appears',
    question: 'My UI does not appear on screen. What do I check?',
    answer:
      'Confirm PrismaUI_F4.dll is installed in F4SE/Plugins and loaded, null-check RequestPluginAPI, verify the HTML path under Data/PrismaUI_F4/views/, and check that you did not immediately Hide() the view without later calling Show(). CreateView starts visible.',
    keywords: ['nothing appears', 'blank', 'not showing', 'invisible', 'view not visible'],
    link: '/docs/troubleshooting',
    linkLabel: 'Troubleshooting',
  },
  {
    id: 'input-not-working',
    question: 'The UI appears but input does not work',
    answer:
      'A visible view is not necessarily the focused interactive panel. Call Focus(view) after Show(view) when the panel should take normal input. V10 also exposes FocusOverlay and optional input-region support for more selective interaction.',
    keywords: ['input', 'keyboard', 'mouse', 'not working', 'focus', 'click', 'input regions'],
    link: '/docs/troubleshooting',
    linkLabel: 'Troubleshooting',
  },
  {
    id: 'listener-does-nothing',
    question: 'My close button or JS listener does nothing',
    answer:
      'Check that the C++ listener name matches the JavaScript callback name exactly, including case. Register listeners at the correct lifecycle point and use RegisterConsoleCallback plus PrismaUI_F4.log to catch JavaScript errors.',
    keywords: ['listener', 'button', 'not firing', 'jslistener', 'callback'],
    link: '/docs/troubleshooting',
    linkLabel: 'Troubleshooting',
  },
  {
    id: 'game-crashes',
    question: 'The game crashes on load or during gameplay',
    answer:
      'First null-check RequestPluginAPI before calling through the returned interface. Also make sure the plugin was built against the current API header and is running on a supported Fallout runtime.',
    keywords: ['crash', 'ctd', 'freeze', 'null pointer'],
    link: '/docs/troubleshooting',
    linkLabel: 'Troubleshooting',
  },
  {
    id: 'looks-unstyled',
    question: 'My UI looks wrong or unstyled',
    answer:
      'Bundle required CSS, JavaScript, fonts, and images with the mod and use local/document-relative URLs. Do not rely on CDN-hosted styles, Google Fonts, or other remote dependencies in the 2.1.0 Ultralight production path.',
    keywords: ['unstyled', 'css not loading', 'looks wrong', 'broken layout', 'font', 'cdn'],
    link: '/docs/networking',
    linkLabel: 'Networking and local assets',
  },
  {
    id: 'devtools-wont-open',
    question: 'Where did the old legacy-runtime DevTools workflow go?',
    answer:
      'PrismaUI_F4 2.1.0 no longer runs the legacy-runtime host/subprocess, so do not rely on the old external previous browser runtime DevTools workflow. Use PrismaUI_F4.log, your plugin log, RegisterConsoleCallback, and normal front-end testing outside the game.',
    keywords: ['devtools', 'inspector', 'debug', 'console', 'legacy-runtime'],
    link: '/docs/troubleshooting',
    linkLabel: 'Troubleshooting',
  },
  {
    id: 'model-preview-blank',
    question: '3D model preview shows nothing',
    answer:
      'Double-check the FormID, source plugin, form type, and runtime support. ModelPreview is native framework functionality, so the most useful first evidence is PrismaUI_F4.log plus the consumer plugin log.',
    keywords: ['3d model', 'model preview', 'blank', 'weapon preview', 'item preview'],
    link: '/docs/model-preview',
    linkLabel: 'Model Preview',
  },
  {
    id: 'input-leaks',
    question: 'Input leaks through to the game while my UI is open',
    answer:
      'Passive widgets are intentionally able to remain visible without owning normal panel focus. For an interactive panel use Focus(), declare an appropriate ViewRole, and use V10 FocusOverlay/input regions only when that behavior is what you need.',
    keywords: ['input leak', 'still moving', 'not blocking input', 'focus'],
    link: '/docs/troubleshooting',
    linkLabel: 'Troubleshooting',
  },
  {
    id: 'hud-suppression-not-working',
    question: 'Vanilla HUD widget suppression is not working',
    answer:
      'In 2.1.0 SuppressHUDWidget supports OG 1.10.163 and AE 1.11.137+ when matching Address Library data is available. Intermediate 1.10.980-1.10.984 Next-Gen is not a supported PrismaUI runtime.',
    keywords: ['hud suppression', 'suppress hud', 'hide widget', 'og', 'ae', 'address library'],
    link: '/docs/vanilla-ui-suppression',
    linkLabel: 'Vanilla UI suppression',
  },
  {
    id: 'cpp-js-communication',
    question: 'How do C++ and JS talk to each other?',
    answer:
      'Use InteropCall for C++ to JS function calls, Invoke for script evaluation/reads, and RegisterJSListener or BindUIEvent for JS to C++ events. Check the API reference for threading and lifecycle requirements.',
    keywords: ['interop', 'communication', 'bridge', 'invoke', 'js listener', 'c++ to js'],
    link: '/docs/api-reference',
    linkLabel: 'API Reference',
  },
  {
    id: 'papyrus-bridge',
    question: 'Can I read or write Papyrus data from JavaScript?',
    answer:
      'Yes. PrismaUI includes Papyrus bridge helpers for supported game data/property access. Use the dedicated guide and keep engine-heavy logic in native code when appropriate.',
    keywords: ['papyrus', 'globals', 'script properties', 'actor values'],
    link: '/docs/papyrus-bridge',
    linkLabel: 'Papyrus bridge',
  },
  {
    id: 'game-versions',
    question: 'Which Fallout 4 versions does PrismaUI_F4 2.1.0 support?',
    answer:
      'Desktop 2.1.0 supports OG 1.10.163 and the AE 1.11.137+ family when matching Address Library data is available. The intermediate 1.10.980-1.10.984 Next-Gen line is deliberately rejected.',
    keywords: ['next-gen', 'ng', 'ae', 'og', 'runtime', 'game version', '1.10.163', '1.11'],
    link: '/docs/1.0-vs-2.0',
    linkLabel: '1.0 vs current',
  },
  {
    id: 'networking-allowed',
    question: 'Can my UI make network requests?',
    answer:
      'Do not design a 2.1.0 Prisma view around direct remote networking. Required assets should be local, worker-style browser APIs are blocked, and real HTTP/WebSocket work should live in your native plugin and be relayed to the view through the Prisma bridge.',
    keywords: ['network', 'fetch', 'websocket', 'xhr', 'cdn', 'sandbox'],
    link: '/docs/networking',
    linkLabel: 'Networking',
  },
  {
    id: 'view-health',
    question: 'How do I detect if a view failed to load or became unhealthy?',
    answer:
      'Use GetViewHealth and the ViewHealth states exposed by the API, together with PrismaUI_F4.log and RegisterConsoleCallback. Do not assume a view remains healthy forever.',
    keywords: ['view health', 'watchdog', 'crashed view', 'unresponsive', 'load failed'],
    link: '/docs/view-watchdog',
    linkLabel: 'View Watchdog',
  },
  {
    id: 'multiple-panels',
    question: 'How do multiple plugins avoid stepping on each other’s UI?',
    answer:
      'Declare a view role with SetViewRole. IsAnyPanelVisible and GetFocusedView let cooperating plugins determine whether another interactive panel is already active before opening their own.',
    keywords: ['multiple plugins', 'panel management', 'coordination', 'viewrole'],
    link: '/docs/panel-management',
    linkLabel: 'Panel management',
  },
  {
    id: 'input-regions',
    question: 'How do V10 input regions work?',
    answer:
      'The current V10 header exposes FocusOverlay and SetInputRegions plus the InputRegions capability bit. Check HasPrismaCapability(PrismaCapability::InputRegions) before making that optional capability a hard dependency.',
    keywords: ['input regions', 'focusoverlay', 'v10', 'capability', 'click through'],
    link: '/docs/api-reference',
    linkLabel: 'API Reference',
  },
  {
    id: 'support-discord',
    question: 'Where can I get help if this FAQ does not cover it?',
    answer:
      'Join the Discord server or open a GitHub issue with the relevant PrismaUI_F4.log, consumer plugin log, framework version, and Fallout runtime.',
    keywords: ['help', 'support', 'discord', 'contact', 'community'],
    link: undefined,
  },
];
