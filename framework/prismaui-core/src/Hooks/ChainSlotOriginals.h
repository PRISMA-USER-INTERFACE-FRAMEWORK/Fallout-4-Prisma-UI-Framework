#pragma once

struct IDXGISwapChain;

namespace PrismaUI::Hooks {
    bool ChainSlotOriginalOwners(IDXGISwapChain* swapChain, void** present, void** resize);
}
