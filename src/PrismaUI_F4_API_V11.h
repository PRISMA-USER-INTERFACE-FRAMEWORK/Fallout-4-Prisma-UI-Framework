#pragma once

#include "PrismaUI_F4_API.h"

namespace PRISMA_UI_API {
using V11DispatchToGameThread = decltype(&IVPrismaUI11::DispatchToGameThread);
using V11BindGameThreadUIEvent = decltype(&IVPrismaUI11::BindGameThreadUIEvent);
}
