#pragma once

namespace PrismaUI::ActorPreviewSpike {

#if defined(PRISMAUI_FO4VR)

void Tick();
void Shutdown();
#else

void Tick();

void Shutdown();
#endif

}
