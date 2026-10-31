// Loaded as a classic <script> by dock.html and evaluated against a bare global by
// tests/dock/dockManifest.test.mjs. Keep it dependency-free and side-effect-free.
(function (root) {
  "use strict";

  var PLACEHOLDER_THUMB = "../dockimage.webp";
  var PRISMA_NEXUS_URL = "https://www.nexusmods.com/fallout4/mods/105454";
  var UNAUTHORED_DESCRIPTION =
    "The author of this mod hasn't set up a page yet.";

  function isNonBlankString(value) {
    return typeof value === "string" && value.trim().length > 0;
  }

  function dirOf(url) {
    var slash = url.lastIndexOf("/");
    return slash === -1 ? "" : url.slice(0, slash);
  }

  function modFolderFromThumb(thumb) {
    if (!isNonBlankString(thumb)) return null;
    var dir = dirOf(thumb);
    if (!dir) return null;
    var slash = dir.lastIndexOf("/");
    var name = slash === -1 ? dir : dir.slice(slash + 1);
    return name.length > 0 ? name : null;
  }

  // Listed when the mod has a LIVE view, not when that view happens to be on screen. The payload
  // only carries views that exist right now, so a mod that never loaded is absent already.
  //
  // Filtering on `hidden` instead was wrong in a way unit tests could not show: the dock is only
  // reachable from inside the ESC menu, and plugins routinely hide their views whenever a menu
  // opens. FalloutChat creates its view hidden and re-hides it on menu open, so it was hidden every
  // single time the dock could be looked at, and could never appear.
  function groupViewsByMod(views) {
    var list = Array.isArray(views) ? views : [];
    var order = [];
    var byMod = Object.create(null);
    for (var i = 0; i < list.length; i++) {
      var v = list[i];
      if (!v) continue;
      var mod = modFolderFromThumb(v.thumb);
      if (!mod) continue;
      if (!Object.prototype.hasOwnProperty.call(byMod, mod)) {
        byMod[mod] = { mod: mod, title: v.title || mod, thumb: v.thumb, viewIds: [], titled: !v.hidden };
        order.push(mod);
      } else if (!v.hidden && !byMod[mod].titled) {
        // Prefer a visible view's title: a mod's hidden helper views are often named less usefully
        // than the one the player actually sees.
        byMod[mod].title = v.title || byMod[mod].title;
        byMod[mod].titled = true;
      }
      byMod[mod].viewIds.push(v.id);
    }
    return order.map(function (m) {
      var e = byMod[m];
      delete e.titled;
      return e;
    });
  }

  // Manifest asset paths are relative to the mod's own view folder. Anything that could reach
  // outside it, or that names a remote resource, is refused: remote images are blocked on the
  // request path anyway, so accepting one would only render a broken image.
  function isSafeRelativeAsset(value) {
    if (!isNonBlankString(value)) return false;
    var s = value.trim();
    if (/^[a-zA-Z][a-zA-Z0-9+.-]*:/.test(s)) return false;
    if (s.charAt(0) === "/" || s.charAt(0) === "\\") return false;
    var parts = s.split(/[\\/]+/);
    for (var i = 0; i < parts.length; i++) {
      if (parts[i] === "..") return false;
    }
    return true;
  }

  function resolveImage(thumb, rawImage) {
    var fallback = isNonBlankString(thumb) ? thumb : PLACEHOLDER_THUMB;
    if (!isSafeRelativeAsset(rawImage)) return fallback;
    var dir = isNonBlankString(thumb) ? dirOf(thumb) : "";
    if (!dir) return fallback;
    return dir + "/" + rawImage.trim().replace(/^[\\/]+/, "").replace(/\\/g, "/");
  }

  function isHttpUrl(value) {
    if (!isNonBlankString(value)) return false;
    try {
      var u = new URL(value.trim());
      var h = u.hostname.toLowerCase();
      return u.protocol === "https:" &&
        (h === "nexusmods.com" || h.slice(-14) === ".nexusmods.com");
    } catch (e) {
      return false;
    }
  }

  function buildCard(entry, manifest) {
    var e = entry || {};
    var m = manifest && typeof manifest === "object" && !Array.isArray(manifest) ? manifest : {};

    var name = isNonBlankString(m.name) ? m.name.trim() : (e.title || String(e.mod || ""));
    var description = isNonBlankString(m.description) ? m.description.trim() : UNAUTHORED_DESCRIPTION;
    var image = resolveImage(e.thumb, m.image);
    // A missing/invalid mod link means the card has no external link. Never substitute Prisma's own
    // Nexus page here or the Dock sends users to the framework page as if it belonged to the mod.
    var nexus = isHttpUrl(m.nexus) ? m.nexus.trim() : null;

    var authored =
      isNonBlankString(m.name) ||
      isNonBlankString(m.description) ||
      isSafeRelativeAsset(m.image) ||
      isHttpUrl(m.nexus);

    return {
      mod: e.mod,
      name: name,
      description: description,
      image: image,
      nexus: nexus,
      authored: authored,
      viewIds: e.viewIds || [],
    };
  }

  root.PrismaDockManifest = {
    PLACEHOLDER_THUMB: PLACEHOLDER_THUMB,
    PRISMA_NEXUS_URL: PRISMA_NEXUS_URL,
    UNAUTHORED_DESCRIPTION: UNAUTHORED_DESCRIPTION,
    modFolderFromThumb: modFolderFromThumb,
    groupViewsByMod: groupViewsByMod,
    isSafeRelativeAsset: isSafeRelativeAsset,
    resolveImage: resolveImage,
    buildCard: buildCard,
  };
})(typeof self !== "undefined" ? self : this);
