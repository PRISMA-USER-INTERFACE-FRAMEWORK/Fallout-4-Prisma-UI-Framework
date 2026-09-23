#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace PrismaUI::LocalMapSpike {

#if defined(PRISMAUI_FO4VR)

void Tick(ID3D11Device* dev, ID3D11DeviceContext* ctx);
#elif defined(PRISMA_DIAGNOSTICS)
void Tick(ID3D11Device* dev, ID3D11DeviceContext* ctx);
#else
inline void Tick(ID3D11Device*, ID3D11DeviceContext*) {}
#endif

}