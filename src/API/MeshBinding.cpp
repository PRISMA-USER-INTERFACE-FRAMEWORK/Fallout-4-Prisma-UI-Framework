#include "MeshBinding.h"

#include "Engine/EngineInterface3D.h"
#include "Engine/EnginePrivateTexture.h"
#include "Engine/EngineTexture.h"
#include "Utils/ModulePath.h"

#include <windows.h>

#include <d3d11.h>
#include <wrl/client.h>

#include "RE/B/BSGeometry.h"
#include "RE/B/BSLightingShaderMaterialBase.h"
#include "RE/B/BSLightingShaderProperty.h"
#include "RE/B/BSShaderProperty.h"
#include "RE/I/Interface3D.h"
#include "RE/N/NiNode.h"
#include "RE/N/NiTexture.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <format>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace PrismaUI::MeshBinding
{
namespace
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

	bool IsEffectShaderProperty(RE::NiObject* obj)
	{
		if (!obj) return false;
		static REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE::BSEffectShaderProperty[0] };
		return *reinterpret_cast<std::uintptr_t*>(obj) == vtbl.address();
	}

	void DumpScreenGraph(RE::NiAVObject* root)
	{
		if (!root) return;
		logger::info("[DIAG-GRAPH] ---- begin scenegraph dump ----");
		auto visit = [](RE::NiAVObject* node, int depth) {
			if (auto* geom = node->IsGeometry()) {
				for (int i = 0; i < 2; ++i) {
					auto* prop = geom->properties[i].get();
					if (!prop) continue;
					const char* kind = "other";
					std::string tex;
					if (auto* lp = netimmerse_cast<RE::BSLightingShaderProperty*>(prop); lp && lp->material) {
						kind = "LIGHTING";
						auto* mat = static_cast<RE::BSLightingShaderMaterialBase*>(lp->material);
						auto* t = mat->diffuseTexture.get();

						tex = std::format("feature={} type={} flags=0x{:016x} diffuse={}",
						                  static_cast<int>(mat->GetFeature()), static_cast<int>(mat->GetType()),
						                  lp->flags.underlying(),
						                  (t && t->name.c_str()) ? t->name.c_str() : "(null)");
					} else if (IsEffectShaderProperty(prop)) {
						kind = "EFFECT";
					} else {
						continue;
					}
					logger::info("[DIAG-GRAPH] d={} geom='{}' prop[{}]={} culled={} {}", depth,
					             geom->name.c_str() ? geom->name.c_str() : "?", i, kind,
					             geom->GetAppCulled(), tex);
				}
			}
			return true;
		};
		ForEachNode(root, visit);
		logger::info("[DIAG-GRAPH] ---- end scenegraph dump ----");
	}

	bool ContainsNoCase(const char* haystack, const char* needle)
	{
		if (!haystack || !needle || !needle[0]) return false;
		std::string h = haystack, n = needle;
		std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c){ return (char)std::tolower(c); });
		std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c){ return (char)std::tolower(c); });
		return h.find(n) != std::string::npos;
	}

	struct ScreenHit {

		void*                             texture = nullptr;
		RE::BSGeometry*                   geometry = nullptr;
		RE::BSLightingShaderMaterialBase* material = nullptr;
		RE::NiTexture*                    diffuse = nullptr;
		RE::BSLightingShaderProperty*     property = nullptr;
	};

	std::unordered_map<ViewId, Engine::PrivateTextureClone> s_privateTextures;
	std::mutex s_privateTexturesMutex;

	Engine::PrivateTextureClone AcquirePrivateTexture(ViewId view, RE::NiTexture* donor,
	                                                  ID3D11ShaderResourceView* srv)
	{
		std::lock_guard lock(s_privateTexturesMutex);
		auto it = s_privateTextures.find(view);
		if (it != s_privateTextures.end() && it->second.clone) {
			Engine::SetPrivateTextureSRV(it->second.rendererTexture, srv);
			return it->second;
		}

		Engine::PrivateTextureClone fresh = Engine::MakePrivateTextureClone(donor, srv);
		if (fresh.clone) s_privateTextures[view] = fresh;
		return fresh;
	}

	RE::NiTexture* FindDonorTexture(RE::NiAVObject* root)
	{
		RE::NiTexture* donor = nullptr;
		auto visit = [&donor](RE::NiAVObject* node, int) {
			if (auto* geom = node->IsGeometry()) {
				for (int i = 0; i < 2; ++i) {
					auto* prop = geom->properties[i].get();
					if (!prop) continue;
					auto* lp = netimmerse_cast<RE::BSLightingShaderProperty*>(prop);
					if (!lp || !lp->material) continue;
					auto* t = static_cast<RE::BSLightingShaderMaterialBase*>(lp->material)->diffuseTexture.get();
					if (!t || !t->rendererTexture) continue;
					if (ContainsNoCase(t->name.c_str(), "pipboy")) continue;
					donor = t;
					return false;
				}
			}
			return true;
		};
		ForEachNode(root, visit);
		return donor;
	}

	ScreenHit FindScreenGeometry(RE::NiAVObject* root, const char* needle, bool byTexture)
	{
		if (!root || !needle) return {};

		ScreenHit visible;
		ScreenHit culled;

		auto visit = [&](RE::NiAVObject* node, int) {
			if (auto* geom = node->IsGeometry()) {
				const bool nameMatch = !byTexture && geom->name.c_str() && std::strcmp(geom->name.c_str(), needle) == 0;
				if (byTexture || nameMatch) {
					for (int i = 0; i < 2; ++i) {
						auto* prop = geom->properties[i].get();
						if (!prop) continue;

						RE::NiTexture* niTex = nullptr;
						auto* lp = netimmerse_cast<RE::BSLightingShaderProperty*>(prop);
						if (lp && lp->material) {
							niTex = static_cast<RE::BSLightingShaderMaterialBase*>(lp->material)->diffuseTexture.get();
						}
						if (!niTex || !niTex->rendererTexture) continue;

						if (byTexture && !ContainsNoCase(niTex->name.c_str(), needle)) continue;

						ScreenHit hit{ static_cast<void*>(niTex->rendererTexture), geom,
						               static_cast<RE::BSLightingShaderMaterialBase*>(lp->material),
						               niTex, lp };
						if (!geom->GetAppCulled()) {
							visible = hit;
							return false;
						}
						if (!culled.texture) culled = hit;
						break;
					}
				}
			}
			return true;
		};

		ForEachNode(root, visit);
		return visible.texture ? visible : culled;
	}

	void CullScreenOverlays(RE::NiAVObject* root, RE::BSGeometry* bound, std::vector<RE::NiPointer<RE::NiAVObject>>& out)
	{
		auto visit = [bound, &out](RE::NiAVObject* node, int) {
			if (auto* geom = node->IsGeometry(); geom && geom != bound) {
				if (ContainsNoCase(geom->name.c_str(), "screen")) {
					for (int i = 0; i < 2; ++i) {
						if (!IsEffectShaderProperty(geom->properties[i].get())) continue;
						if (!geom->GetAppCulled()) {
							geom->SetAppCulled(true);
							out.push_back(RE::NiPointer<RE::NiAVObject>(geom));
						}
						break;
					}
				}
			}
			return true;
		};
		ForEachNode(root, visit);
	}

	bool ScreenSelfIlluminate()
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

	struct Binding {
		void* texture = nullptr;
		ID3D11ShaderResourceView* original = nullptr;
		std::vector<RE::NiPointer<RE::NiAVObject>> culledOverlays;
		ID3D11ShaderResourceView* weWrote = nullptr;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srvRef;
		RE::NiPointer<RE::BSGeometry> geometry;
		bool wasCulled = false;

		RE::BSLightingShaderMaterialBase* material = nullptr;
		RE::NiPointer<RE::NiTexture> originalDiffuse;
		RE::NiPointer<RE::NiTexture> privateClone;
		RE::NiPointer<RE::NiTexture> textureOwner;
		std::uint32_t matStomps = 0;
		RE::NiPointer<RE::BSLightingShaderProperty> property;
		std::uint64_t originalFlags = 0;
		bool flagsPatched = false;
		std::uint32_t flagStomps = 0;

		RE::BSFixedString detachedRendererName;
		RE::BSFixedString originalScreenGeomName;
		bool detached = false;
	};

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

	std::uint64_t ScreenClearMask()
	{
		return ScreenSelfIlluminate() ? (kScreenFlagsToClear & ~kPipboyScreenFlag) : kScreenFlagsToClear;
	}

	void InvalidateRenderPasses(RE::BSLightingShaderProperty* prop)
	{
		prop->lastRenderPassState = 0x7fffffff;
	}

	void DumpOneRenderer(RE::Interface3D::Renderer* r, RE::BSGeometry* boundGeom, const char* tag)
	{
		if (!r) return;
		bool containsOurs = false;
		std::string names;
		for (const auto& geo : r->displayGeometry) {
			if (geo.get() == boundGeom) containsOurs = true;
			names += (geo && geo->name.c_str()) ? geo->name.c_str() : "(null)";
			names += ' ';
		}
		logger::info("[DIAG-I3D] {} name='{}' enabled={} screenGeomName='{}' screenMaterialName='{}' "
		             "screenMode={} postfx={} omsize={} displayGeometry.count={} containsOurGeom={} [{}]",
		             tag, r->name.c_str() ? r->name.c_str() : "(null)", r->enabled,
		             r->screenGeomName.c_str() ? r->screenGeomName.c_str() : "(null)",
		             r->screenMaterialName.c_str() ? r->screenMaterialName.c_str() : "(null)",
		             static_cast<int>(r->screenmode.underlying()), static_cast<int>(r->postfx.underlying()),
		             static_cast<int>(r->omsize.underlying()), r->displayGeometry.size(),
		             containsOurs, names);
	}

	void DumpInterface3DRenderer(RE::BSGeometry* boundGeom)
	{
		logger::info("[DIAG-I3D] global renderer list: {} entries",
		             Engine::Interface3DRendererCount());
		Engine::ForEachInterface3DRenderer(
			[boundGeom](RE::Interface3D::Renderer* r) { DumpOneRenderer(r, boundGeom, "global"); });
	}

	void DetachFromInterface3D(Binding& b, RE::BSGeometry* geom)
	{
		auto* r = Engine::FindRendererOwning(geom);
		if (!r) return;

		auto& arr = r->displayGeometry;
		for (auto it = arr.begin(); it != arr.end(); ++it) {
			if (it->get() == geom) {
				arr.erase(it);
				break;
			}
		}

		b.detachedRendererName = r->name;
		b.originalScreenGeomName = r->screenGeomName;
		b.detached = true;
		r->screenGeomName = RE::BSFixedString("");

		logger::info("BindViewToGeometry: detached '{}' from Interface3D renderer '{}' "
		             "(displayGeometry={}, screenGeomName '{}' -> blank, screenMaterialName='{}')",
		             geom->name.c_str() ? geom->name.c_str() : "?",
		             r->name.c_str() ? r->name.c_str() : "?", arr.size(),
		             b.originalScreenGeomName.c_str() ? b.originalScreenGeomName.c_str() : "",
		             r->screenMaterialName.c_str() ? r->screenMaterialName.c_str() : "");
	}

	void ReattachToInterface3D(Binding& b)
	{
		if (!b.detached) return;
		auto* r = RE::Interface3D::Renderer::GetByName(b.detachedRendererName);
		if (!r) {
			logger::info("UnbindViewFromGeometry: renderer '{}' already destroyed (self-heals next open)",
			             b.detachedRendererName.c_str() ? b.detachedRendererName.c_str() : "?");
			return;
		}
		r->screenGeomName = b.originalScreenGeomName;
		logger::info("UnbindViewFromGeometry: restored screenGeomName '{}' on Interface3D renderer '{}'",
		             b.originalScreenGeomName.c_str() ? b.originalScreenGeomName.c_str() : "",
		             b.detachedRendererName.c_str() ? b.detachedRendererName.c_str() : "?");
	}

	void PatchScreenShaderFlags(Binding& b, RE::BSLightingShaderProperty* prop)
	{
		if (!prop) return;

		const std::uint64_t clearMask = ScreenClearMask();
		const std::uint64_t flags = prop->flags.underlying();
		if (!(flags & clearMask)) return;
		b.property.reset(prop);
		b.originalFlags = flags;
		b.flagsPatched = true;
		prop->flags = static_cast<RE::BSShaderProperty::EShaderPropertyFlag>(flags & ~clearMask);
		InvalidateRenderPasses(prop);
		logger::info("BindViewToGeometry: cleared 0x{:016x} on bound property "
		             "(flags 0x{:016x} -> 0x{:016x}) + render passes invalidated",
		             clearMask, flags, prop->flags.underlying());
	}

	void ForceVisible(Binding& b)
	{
		if (!b.geometry) return;
		b.wasCulled = b.geometry->GetAppCulled();
		if (b.wasCulled) b.geometry->SetAppCulled(false);
	}

	std::unordered_map<ViewId, Binding> s_meshBindings;
	std::mutex s_meshBindingsMutex;

	Binding MakeBinding(RE::NiAVObject* root, const ScreenHit& hit, ID3D11ShaderResourceView* srv)
	{
		Binding binding;
		binding.srvRef = srv;
		binding.texture = hit.texture;
		binding.original = Engine::PeekEngineTextureSRV(hit.texture);
		binding.weWrote = srv;
		binding.geometry.reset(hit.geometry);
		binding.textureOwner.reset(hit.diffuse);
		CullScreenOverlays(root, hit.geometry, binding.culledOverlays);
		ForceVisible(binding);
		PatchScreenShaderFlags(binding, hit.property);
		DetachFromInterface3D(binding, hit.geometry);
		DumpInterface3DRenderer(hit.geometry);
		return binding;
	}

	void Store(ViewId view, Binding&& binding)
	{
		std::lock_guard lock(s_meshBindingsMutex);
		s_meshBindings[view] = std::move(binding);
	}

}

bool Enabled()
{
	static const bool enabled = [] {

		const auto ini = PrismaUI::Utils::PluginIniPath();
		const bool on = ::GetPrivateProfileIntW(L"Debug", L"bMeshBinding", 0, ini.wstring().c_str()) != 0;
		logger::warn("[MeshBinding] BindViewToGeometry is {} ([Debug] bMeshBinding, ini='{}')",
		             on ? "ENABLED" : "DISABLED", ini.string());
		return on;
	}();
	return enabled;
}

bool BindToGeometry(ViewId view, RE::NiAVObject* root, const char* geometryName,
                    ID3D11ShaderResourceView* srv)
{
	if (!view || !root || !geometryName || !srv) return false;

	Unbind(view);
	DumpScreenGraph(root);
	auto hit = FindScreenGeometry(root, geometryName, false);
	if (!hit.texture) {
		logger::warn("BindViewToGeometry: geometry '{}' has no bindable diffuse texture", geometryName);
		return false;
	}

	Binding binding = MakeBinding(root, hit, srv);

	bool privateTex = false;
	if (hit.material) {
		if (auto* donor = FindDonorTexture(root); donor && donor != hit.diffuse) {
			if (auto priv = AcquirePrivateTexture(view, donor, srv); priv.clone) {
				binding.material = hit.material;
				binding.originalDiffuse = hit.material->diffuseTexture;
				hit.material->diffuseTexture = RE::NiPointer<RE::NiTexture>(priv.clone);
				binding.texture = priv.rendererTexture;
				binding.privateClone.reset(priv.clone);
				privateTex = true;
			}
		}
	}
	if (!privateTex) {
		Engine::PokeEngineTextureSRV(hit.texture, srv);
	}

	const size_t hidden = binding.culledOverlays.size();
	Store(view, std::move(binding));
	logger::info("BindViewToGeometry: view [{}] bound to '{}' ({} overlay(s) hidden) privateTexture={} "
	             "srv={}",
	             view, geometryName, hidden, privateTex, static_cast<void*>(srv));
	return true;
}

bool BindToScreenTexture(ViewId view, RE::NiAVObject* root, const char* textureSubstring,
                         ID3D11ShaderResourceView* srv)
{
	if (!view || !root || !textureSubstring || !srv) return false;

	Unbind(view);
	auto hit = FindScreenGeometry(root, textureSubstring, true);
	if (!hit.texture) {
		logger::warn("BindViewToScreenTexture: no diffuse texture matching '{}'", textureSubstring);
		return false;
	}

	Binding binding = MakeBinding(root, hit, srv);
	Engine::PokeEngineTextureSRV(hit.texture, srv);

	const size_t hidden = binding.culledOverlays.size();
	const auto* boundTexture = binding.texture;
	Store(view, std::move(binding));
	logger::info("BindViewToScreenTexture: view [{}] bound to texture '{}' ({} overlay(s) hidden) "
	             "[DIAG] bsTex={} wroteSRV={}",
	             view, textureSubstring, hidden, static_cast<const void*>(boundTexture),
	             static_cast<void*>(srv));
	return true;
}

void Unbind(ViewId view)
{
	if (!view) return;
	std::lock_guard lock(s_meshBindingsMutex);
	auto it = s_meshBindings.find(view);
	if (it == s_meshBindings.end()) return;
	if (it->second.material) {
		it->second.material->diffuseTexture = it->second.originalDiffuse;
	} else if (it->second.texture) {
		Engine::PokeEngineTextureSRV(it->second.texture, it->second.original);
	}
	for (auto& overlay : it->second.culledOverlays) {
		if (overlay) overlay->SetAppCulled(false);
	}
	if (it->second.geometry && it->second.wasCulled) {
		it->second.geometry->SetAppCulled(true);
	}
	if (it->second.flagsPatched && it->second.property) {
		it->second.property->flags =
			static_cast<RE::BSShaderProperty::EShaderPropertyFlag>(it->second.originalFlags);
		InvalidateRenderPasses(it->second.property.get());
	}
	ReattachToInterface3D(it->second);
	const auto restored = it->second.culledOverlays.size();
	s_meshBindings.erase(it);
	logger::info("UnbindViewFromGeometry: view [{}] restored original texture ({} overlay(s) shown)",
	             view, restored);
}

void DiagTick()
{
	static uint64_t frame = 0;
	const bool probe = (++frame % 120 == 0);
	std::lock_guard lock(s_meshBindingsMutex);

	for (auto& [bview, b] : s_meshBindings) {
		(void)bview;
		if (!b.material || !b.privateClone) continue;
		auto** slot = reinterpret_cast<RE::NiTexture**>(std::addressof(b.material->diffuseTexture));
		if (*slot == b.privateClone.get()) continue;
		auto* engineTex = *slot;
		if (b.matStomps++ < 5) {
			const char* n = (engineTex && engineTex->name.c_str()) ? engineTex->name.c_str() : "(null)";
			logger::info("[DIAG-MESH] MATSTOMP #{}: engine reassigned material diffuse to {} '{}' "
			             "(ours={}) -- re-asserting",
			             b.matStomps, static_cast<void*>(engineTex), n,
			             static_cast<void*>(b.privateClone.get()));
		}
		*slot = b.privateClone.get();
	}

	const std::uint64_t reClearMask = ScreenClearMask();
	for (auto& [fview, b] : s_meshBindings) {
		(void)fview;
		if (!b.flagsPatched || !b.property) continue;
		const std::uint64_t now = b.property->flags.underlying();
		if (!(now & reClearMask)) continue;
		b.flagStomps++;
		b.property->flags =
			static_cast<RE::BSShaderProperty::EShaderPropertyFlag>(now & ~reClearMask);
		InvalidateRenderPasses(b.property.get());
	}

	if (!probe) return;
	for (auto& [view, b] : s_meshBindings) {
		if (!b.texture) continue;
		auto* now = Engine::PeekEngineTextureSRV(b.texture);
		const bool stomped = (now != b.weWrote);
		bool culled = false;
		const char* geomName = "?";
		if (b.geometry) {
			culled = b.geometry->GetAppCulled();
			if (b.geometry->name.c_str()) geomName = b.geometry->name.c_str();
		}

		logger::debug("[DIAG-MESH] view={} geom='{}' culled={} bsTex={} pSRView(now)={} weWrote={} "
		             "original={} STOMPED={} matStomps={} propFlags=0x{:016x} flagStomps={} passState={:#x} "
		             "alpha={:.3f} lodFade={:.3f} matAlpha={:.3f} passList={}",
		             view, geomName, culled, static_cast<void*>(b.texture), static_cast<void*>(now),
		             static_cast<void*>(b.weWrote), static_cast<void*>(b.original),
		             stomped ? "YES" : "no", b.matStomps,
		             b.property ? b.property->flags.underlying() : 0,
		             b.flagStomps,
		             b.property ? b.property->lastRenderPassState : 0,
		             b.property ? b.property->alpha : -1.0f,
		             b.property ? b.property->lodFade : -1.0f,
		             b.property ? b.property->QMaterialAlpha() : -1.0f,
		             b.property ? static_cast<void*>(b.property->renderPassList.passList) : nullptr);
	}
}

}
