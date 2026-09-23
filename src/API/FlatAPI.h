#pragma once

#include "PrismaUI_F4_Modern_API.h"

namespace PrismaUI::FlatAPI {
    bool Get(PRISMA_UI_FLAT_API::ApiFeature feature, uint32_t version, void* table, uint32_t tableSize) noexcept;
    bool GetVR(PRISMA_UI_FLAT_API::ApiFeature feature, uint32_t version, void* table, uint32_t tableSize) noexcept;
}
