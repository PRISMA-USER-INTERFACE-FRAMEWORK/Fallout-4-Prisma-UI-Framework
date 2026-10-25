#pragma once

#include <string>
#include <string_view>

#include "URLWhitelist.h"

namespace PrismaUI::NetworkSandbox {

    namespace detail {

        inline std::string GenerateFetchXhrGuard(std::string_view predicate, std::string_view reason,
                                                 std::string_view errorPrefix) {
            std::string script;
            script.reserve(2200 + predicate.size() * 2 + reason.size() * 2 + errorPrefix.size() * 2);
            script += R"js(
function reportBlocked(reason, url) {
    try { window.__prismaNative && window.__prismaNative('networkBlocked', reason + ':' + url); } catch (e) {}
}

try {
    var nativeFetch = window.fetch;
    if (typeof nativeFetch === 'function') {
        var wrappedFetch = function(input, init) {
            var url = (typeof input === 'string') ? input : (input && input.url);
            if (! )js";
            script += predicate;
            script += R"js((url)) {
                reportBlocked(')js";
            script += reason;
            script += R"js(', url);
                return Promise.reject(new TypeError(')js";
            script += errorPrefix;
            script += R"js(' + url));
            }
            return nativeFetch.call(window, input, init);
        };
        DEF(window, 'fetch', { value: wrappedFetch, writable: false, configurable: false, enumerable: true });
    }
} catch(e) {}

try {
    var NativeXHR = window.XMLHttpRequest;
    if (typeof NativeXHR === 'function') {
        var WrappedXHR = function() {
            var xhr = new NativeXHR();
            var nativeOpen = xhr.open;
            xhr.open = function(method, url) {
                if (! )js";
            script += predicate;
            script += R"js((url)) {
                    reportBlocked(')js";
            script += reason;
            script += R"js(', url);
                    throw new TypeError(')js";
            script += errorPrefix;
            script += R"js(' + url);
                }
                return nativeOpen.apply(xhr, arguments);
            };
            return xhr;
        };
        WrappedXHR.prototype = NativeXHR.prototype;
        DEF(window, 'XMLHttpRequest', { value: WrappedXHR, writable: false, configurable: false, enumerable: true });
    }
} catch(e) {}
)js";
            return script;
        }

    }

    inline std::string InjectContentSecurityPolicyIntoHtml(std::string_view html) {
        const std::string csp = URLWhitelist::GenerateContentSecurityPolicy(URLWhitelist::CspTransport::kUltralight);
        const std::string meta = "<meta http-equiv=\"Content-Security-Policy\" content=\"" + csp + "\">";
        return "<!doctype html><html><head>" + meta + "</head><body>" + std::string{html} + "</body></html>";
    }

    inline std::string InjectInspectorContentSecurityPolicyIntoHtml(std::string_view html) {
        constexpr std::string_view csp =
            "default-src 'self' data: blob:; "
            "connect-src 'none'; "
            "worker-src 'self' blob:; "
            "child-src 'self' blob:; "
            "frame-src 'self' data: blob:; "
            "img-src 'self' data: blob:; "
            "media-src 'self' data: blob:; "
            "font-src 'self' data: blob:; "
            "style-src 'self' 'unsafe-inline'; "
            "script-src 'self' 'unsafe-inline' data:; "
            "object-src 'none'; "
            "base-uri 'none'; "
            "form-action 'none'";
        const std::string meta = "<meta http-equiv=\"Content-Security-Policy\" content=\"" +
                                 std::string{csp} + "\">";
        std::string result{html};
        constexpr std::string_view head = "<head>";
        const auto position = result.find(head);
        if (position == std::string::npos) {
            result.insert(0, meta);
        } else {
            result.insert(position + head.size(), meta);
        }
        return result;
    }

    inline std::string GenerateScript() {
        std::string script = R"js(
(function(){
'use strict';
var DEF = Object.defineProperty.bind(Object);
var DEAD = { value: undefined, writable: false, configurable: false, enumerable: true };

var WHITELIST =)js" + PrismaUI::URLWhitelist::GenerateJsConnectWhitelistArray() +
                             R"js(;

function isWhitelistedHost(host) {
    if (!host) return false;
    host = host.toLowerCase();
    for (var i = 0; i < WHITELIST.length; ++i) {
        var w = WHITELIST[i];
        if (host === w) return true;
        if (host.length > w.length &&
            host.slice(-(w.length + 1)) === ('.' + w)) return true;
    }
    return false;
}

function isAllowedUrl(url) {
    try {
        var u = new URL(url, window.location.href);

        // prisma: is the framework's own scheme, served off disk by PrismaSchemeHandler, which does
        // its own path validation and refuses '..'. It reaches no network at all. It has to be
        // allowed explicitly rather than falling out of the same-origin check, because prisma://shell
        // and each isolated prisma://view-<id> origin are different origins. Native request filtering
        // still verifies that a view only reads its own plugin folder or shared framework assets.
        if (u.protocol === 'prisma:') return true;
        if (u.origin === window.location.origin) return true;
        return u.protocol === 'https:' && isWhitelistedHost(u.hostname);
    } catch (e) {
        return false;
    }
}
)js";

        script += detail::GenerateFetchXhrGuard("isAllowedUrl", "whitelist",
                                                "Blocked by PrismaUI network sandbox (not whitelisted): ");

        script += R"js(
var BLOCKED = [
    'WebSocket',
    'EventSource',
    'Worker',
    'SharedWorker',
    'WebTransport',
    'RTCPeerConnection'
];
for (var i = 0; i < BLOCKED.length; ++i) {
    try { DEF(window, BLOCKED[i], DEAD); } catch(e) {}
}

try {
    DEF(navigator, 'sendBeacon', { value: function(){ return false; }, writable: false, configurable: false });
} catch(e) {}
try {
    DEF(navigator, 'serviceWorker', DEAD);
} catch(e) {}

try {
    var meta = document.createElement('meta');
    meta.setAttribute('http-equiv', 'Content-Security-Policy');
    meta.setAttribute('content', ")js" +
                  PrismaUI::URLWhitelist::GenerateContentSecurityPolicy() + R"js(");
    if (document.head) {
        document.head.insertBefore(meta, document.head.firstChild);
    }
} catch(e) {}
})();
)js";
        return script;
    }

    inline std::string GeneratePrivateNetworkGuardScript() {
        std::string script = R"js(
(function(){
'use strict';
var DEF = Object.defineProperty.bind(Object);

function isPrivateIPv4(host) {
    var m = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/.exec(host);
    if (!m) return false;
    var a = +m[1], b = +m[2];
    if (a > 255 || b > 255 || +m[3] > 255 || +m[4] > 255) return false;
    if (a === 127) return true;
    if (a === 10) return true;
    if (a === 172 && b >= 16 && b <= 31) return true;
    if (a === 192 && b === 168) return true;
    if (a === 169 && b === 254) return true;
    if (a === 100 && b >= 64 && b <= 127) return true;
    if (a === 198 && (b === 18 || b === 19)) return true;
    if (a >= 224) return true;
    if (a === 0) return true;
    return false;
}

function isPrivateHost(rawHost) {
    var host = (rawHost || '').toLowerCase().replace(/\.+$/, '');
    if (!host) return true;
    if (host === 'localhost' || /\.(localhost|local|localdomain|internal|lan|home)$/.test(host)) return true;

    var bare = (host.charAt(0) === '[' && host.charAt(host.length - 1) === ']')
        ? host.slice(1, -1) : host;
    bare = bare.split('%')[0];
    if (bare === '::1' || bare === '::') return true;
    if (bare.indexOf(':') !== -1) {
        var first = parseInt((bare.split(':')[0] || '0'), 16);
        if ((first & 0xffc0) === 0xfe80) return true; // fe80::/10
        if ((first & 0xfe00) === 0xfc00) return true; // fc00::/7
        if ((first & 0xff00) === 0xff00) return true; // multicast
        if (bare.indexOf('2001:db8:') === 0) return true;
    }
    if (bare.indexOf('.') === -1 && bare.indexOf(':') === -1) return true;
    return isPrivateIPv4(bare);
}

function isSafeUrl(url) {
    try {
        var u = new URL(url, window.location.href);
        return !isPrivateHost(u.hostname);
    } catch (e) {
        return false;
    }
}
)js";

        script +=
            detail::GenerateFetchXhrGuard("isSafeUrl", "privatenet", "Blocked by PrismaUI private-network guard: ");
        script += R"js(
})();
)js";
        return script;
    }

}
