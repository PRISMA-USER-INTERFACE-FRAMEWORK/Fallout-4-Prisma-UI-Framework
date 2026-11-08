#include "PrismaUI_F4_API.h"
#include "PrismaUI/WebInput.h"

extern "C" __declspec(dllexport) bool PrismaUI_F4_SetViewCursorPolicy(
    PrismaView view,
    PRISMA_UI_API::CursorPolicy policy) noexcept
{
    return PrismaUI::WebInput::SetViewCursorPolicy(view, static_cast<std::uint32_t>(policy));
}

extern "C" __declspec(dllexport) PRISMA_UI_API::CursorPolicy PrismaUI_F4_GetViewCursorPolicy(
    PrismaView view) noexcept
{
    return static_cast<PRISMA_UI_API::CursorPolicy>(PrismaUI::WebInput::GetViewCursorPolicy(view));
}
