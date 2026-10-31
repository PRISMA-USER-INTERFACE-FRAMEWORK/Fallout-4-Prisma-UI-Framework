#pragma once

#include "../PrismaUI/URLWhitelist.h"

namespace PrismaUI::Engine::ResourceTypePolicy {

enum RawResourceType : int {
    kMainFrame = 0,
    kSubFrame = 1,
    kStylesheet = 2,
    kScript = 3,
    kImage = 4,
    kFontResource = 5,
    kSubResource = 6,
    kObject = 7,
    kMedia = 8,
    kWorker = 9,
    kSharedWorker = 10,
    kPrefetch = 11,
    kFavicon = 12,
    kXhr = 13,
    kPing = 14,
    kServiceWorker = 15,
    kCspReport = 16,
    kPluginResource = 17,
    kNavigationPreloadMainFrame = 19,
    kNavigationPreloadSubFrame = 20,
};

inline PrismaUI::URLWhitelist::Capability Classify(const int rawResourceType) {
    using Capability = PrismaUI::URLWhitelist::Capability;
    switch (rawResourceType) {
    case kScript:
    case kWorker:
    case kSharedWorker:
    case kServiceWorker:
    case kObject:
        return Capability::kScript;

    case kStylesheet:    return Capability::kStylesheet;
    case kFontResource:  return Capability::kFont;
    case kImage:
    case kFavicon:
        return Capability::kImage;
    case kMedia:         return Capability::kMedia;

    case kXhr:
    case kPing:
    case kCspReport:
        return Capability::kConnect;

    case kSubFrame:
    case kNavigationPreloadSubFrame:
        return Capability::kFrame;
    case kMainFrame:
    case kNavigationPreloadMainFrame:
        return Capability::kDocument;

    default:
        return Capability::kUnknown;
    }
}

}
