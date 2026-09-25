#pragma once

#if defined(PRISMA_MODELPREVIEW_INTERFACE3D_PROTOTYPE)

namespace RE {
    class NiAVObject;
}
struct ID3D11ShaderResourceView;

namespace PrismaUI::ModelPreviewInterface3D {

    ID3D11ShaderResourceView* RenderModelToSRV(RE::NiAVObject* model);

    void Teardown();

}

#endif
