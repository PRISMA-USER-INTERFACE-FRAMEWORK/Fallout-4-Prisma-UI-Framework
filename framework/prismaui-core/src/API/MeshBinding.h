#pragma once

#include "API/MeshBindingReadinessPolicy.h"

#include <wrl/client.h>

#include <cstdint>

struct ID3D11ShaderResourceView;

namespace RE
{
	class NiAVObject;
}

namespace PrismaUI::MeshBinding
{
	using ViewId = std::uint64_t;
	using Readiness::Availability;

	bool Enabled();

	[[nodiscard]] Availability GetAvailability();

	bool BindToGeometry(ViewId view, RE::NiAVObject* root, const char* geometryName,
	                    ID3D11ShaderResourceView* srv);

	bool BindToScreenTexture(ViewId view, RE::NiAVObject* root, const char* textureSubstring,
	                         ID3D11ShaderResourceView* srv);

	void Unbind(ViewId view);

	void DiagTick(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> (*currentSrv)(ViewId));
}
