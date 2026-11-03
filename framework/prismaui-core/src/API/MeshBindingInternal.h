#pragma once

#include "MeshBinding.h"
#include "MeshBindingSrvPolicy.h"

#include "Utils/ModulePath.h"

#include <windows.h>

#include <d3d11.h>
#include <wrl/client.h>

#include "RE/B/BSGeometry.h"
#include "RE/B/BSLightingShaderProperty.h"
#include "RE/B/BSShaderProperty.h"
#include "RE/N/NiNode.h"
#include "RE/N/NiTexture.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace PrismaUI::MeshBinding::Internal
{
	template <class F>
	bool ForEachNode(RE::NiAVObject* node, F& fn, int depth = 0)
	{
		if (!node) return true;
		if (!fn(node, depth)) return false;
		if (auto* niNode = node->IsNode()) {
			for (const auto& child : niNode->children) {
				if (child && !ForEachNode(child.get(), fn, depth + 1)) return false;
			}
		}
		return true;
	}

	inline bool IsEffectShaderProperty(RE::NiObject* obj)
	{
		if (!obj) return false;
		static REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE::BSEffectShaderProperty[0] };
		return *reinterpret_cast<std::uintptr_t*>(obj) == vtbl.address();
	}

	inline bool ScreenSelfIlluminate()
	{
		static const bool on = [] {
			const auto ini = PrismaUI::Utils::PluginIniPath().wstring();
			const bool v = ::GetPrivateProfileIntW(L"MeshBinding", L"bScreenSelfIlluminate", 0, ini.c_str()) != 0;
			logger::warn("[MeshBinding] screen self-illumination is {} ([MeshBinding] bScreenSelfIlluminate)",
			             v ? "ON (keeping kPipboyScreen)" : "OFF (scene-lit, default)");
			return v;
		}();
		return on;
	}

	constexpr std::uint64_t kPipboyScreenFlag =
		static_cast<std::uint64_t>(RE::BSShaderProperty::EShaderPropertyFlag::kPipboyScreen);

	constexpr std::uint64_t kVertexColorsFlag =
		static_cast<std::uint64_t>(RE::BSShaderProperty::EShaderPropertyFlag::kVertexColors);

	constexpr std::uint64_t kSpecularFlag =
		static_cast<std::uint64_t>(RE::BSShaderProperty::EShaderPropertyFlag::kSpecular);

	constexpr std::uint64_t kEnvMapFlag =
		static_cast<std::uint64_t>(RE::BSShaderProperty::EShaderPropertyFlag::kEnvMap);

	constexpr std::uint64_t kScreenFlagsToClear =
		kPipboyScreenFlag | kVertexColorsFlag | kSpecularFlag | kEnvMapFlag;

	inline std::uint64_t ScreenClearMask()
	{
		return ScreenSelfIlluminate() ? (kScreenFlagsToClear & ~kPipboyScreenFlag) : kScreenFlagsToClear;
	}

	inline void InvalidateRenderPasses(RE::BSLightingShaderProperty* prop)
	{
		prop->lastRenderPassState = 0x7fffffff;
	}

	struct Binding {
		void* texture = nullptr;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> original;
		std::vector<RE::NiPointer<RE::NiAVObject>> culledOverlays;
		ID3D11ShaderResourceView* weWrote = nullptr;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srvRef;
		RE::NiPointer<RE::BSGeometry> geometry;
		bool wasCulled = false;

		RE::NiPointer<RE::NiTexture> textureOwner;
		RE::NiPointer<RE::BSLightingShaderProperty> property;
		std::uint64_t originalFlags = 0;
		bool flagsPatched = false;
		std::uint32_t flagStomps = 0;

		RE::BSFixedString detachedRendererName;
		RE::BSFixedString originalScreenGeomName;
		bool detached = false;
	};

	inline std::unordered_map<ViewId, Binding> s_meshBindings;
	inline std::mutex s_meshBindingsMutex;
	inline std::atomic<std::size_t> s_activeBindings{ 0 };

	void DumpScreenGraph(RE::NiAVObject* root);
	void DumpInterface3DRenderer(RE::BSGeometry* boundGeom);
}
