#pragma once

#include "Utils/ModulePath.h"

#pragma warning(push)
#pragma warning(disable : 4100)
#include <Ultralight/Ultralight.h>
#pragma warning(pop)

#include <windows.h>

#include <algorithm>
#include <cstdint>

namespace PrismaUI::WebRuntimeUltralight {

    inline uint32_t ScaledMemorySetting(UINT raw, uint32_t unit) {
        return static_cast<int>(raw) <= 0 ? 0 : (std::min)(static_cast<uint32_t>(raw), UINT32_MAX / unit) * unit;
    }

    template <class Config>
    inline void ApplyMemoryTuning(Config& config) {
        const auto ini = Utils::PluginIniPath().wstring();
        const auto* path = ini.c_str();
        constexpr uint32_t kMiB = 1024 * 1024;
        if (const auto v = ScaledMemorySetting(::GetPrivateProfileIntW(L"Memory", L"iOverrideRamMB", 1024, path), kMiB))
            config.override_ram_size = v;
        if (const auto v = ScaledMemorySetting(::GetPrivateProfileIntW(L"Memory", L"iMinLargeHeapMB", 8, path), kMiB))
            config.min_large_heap_size = v;
        if (const auto v = ScaledMemorySetting(::GetPrivateProfileIntW(L"Memory", L"iMinSmallHeapKB", 512, path), 1024))
            config.min_small_heap_size = v;
        if (const auto v = ScaledMemorySetting(::GetPrivateProfileIntW(L"Memory", L"iMemoryCacheMB", 32, path), kMiB))
            config.memory_cache_size = v;
        config.page_cache_size = ScaledMemorySetting(::GetPrivateProfileIntW(L"Memory", L"iPageCacheSize", 0, path), 1);
        config.num_renderer_threads = ScaledMemorySetting(::GetPrivateProfileIntW(L"Memory", L"iRendererThreads", 2, path), 1);
    }

}
