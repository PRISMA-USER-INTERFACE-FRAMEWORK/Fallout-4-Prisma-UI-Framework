import { defineConfig } from "vite";

// Phase 3 hello-world milestone: the shell is the ONE top-level page the native host navigates
// to; every Prisma "view" becomes a dynamically-created <iframe> inside it (see design proposal,
// C:\Users\Game\.claude\plans\linear-gliding-crown.md). Relative base + inlined assets so the
// built output works from a plain file:// URL with no dev server, as the native host loads it.
export default defineConfig({
  base: "./",
  build: {
    outDir: "dist",
    assetsInlineLimit: 100000000,
    rollupOptions: {
      output: {
        inlineDynamicImports: true,
      },
    },
  },
});
