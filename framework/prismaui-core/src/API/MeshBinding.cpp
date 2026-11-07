#include "MeshBinding.h"
#include "MeshBindingInternal.h"

#include "Engine/EngineInterface3D.h"
#include "Engine/EngineTexture.h"
#include "Engine/EngineTextureLayoutRuntime.h"
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
#include <atomic>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <format>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace PrismaUI::MeshBinding
{
using namespace Internal;
namespace
{

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
		auto* r = Engine::FindRendererByName(b.detachedRendererName);
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

	Binding MakeBinding(RE::NiAVObject* root, const ScreenHit& hit, ID3D11ShaderResourceView* srv)
	{
		Binding binding;
		binding.srvRef = srv;
		binding.texture = hit.texture;
		// Invariant: original holds its own reference on the borrowed engine SRV (see code notes).
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

	// Takes s_meshBindingsMutex. Returns the view that already owns the texture, or 0; the answer can go stale before Store (#17).
	ViewId FindBoundOwner(const void* texture, ViewId candidate)
	{
		std::lock_guard lock(s_meshBindingsMutex);
		for (const auto& [owner, binding] : s_meshBindings) {
			if (SrvPolicy::ConflictsWithOtherView(owner, candidate, binding.texture, texture)) return owner;
		}
		return 0;
	}

	void Store(ViewId view, Binding&& binding)
	{
		std::lock_guard lock(s_meshBindingsMutex);
		// Ordering: the poke and the store happen under one lock hold (see code notes).
		void* const texture = binding.texture;
		ID3D11ShaderResourceView* const wrote = binding.weWrote;
		s_meshBindings[view] = std::move(binding);
		Engine::PokeEngineTextureSRV(texture, wrote);
		s_activeBindings.store(s_meshBindings.size(), std::memory_order_release);
	}

}

Availability GetAvailability()
{
	static const auto availability = [] {

		const auto ini = PrismaUI::Utils::PluginIniPath();
		const bool on = ::GetPrivateProfileIntW(L"Debug", L"bMeshBinding", 0, ini.wstring().c_str()) != 0;
		const auto resolved = Readiness::ResolveAvailability(on, Engine::TextureLayoutBuildVerified());
		logger::warn("[MeshBinding] mesh binding is {} ([Debug] bMeshBinding={}, ini='{}')",
		             Readiness::AvailabilityReason(resolved), on ? 1 : 0, ini.string());
		return resolved;
	}();
	return availability;
}

bool Enabled()
{
	return GetAvailability() == Availability::Available;
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

	if (hit.material) {
		if (auto* donor = FindDonorTexture(root); donor && donor != hit.diffuse) {
			logger::warn("BindViewToGeometry: geometry '{}' would need a private texture; refusing to bind (fail closed)",
			             geometryName);
			return false;
		}
	}

	// Constraint: fail closed on a double bind (see code notes).
	if (const ViewId owner = FindBoundOwner(hit.texture, view)) {
		logger::warn("BindViewToGeometry: geometry '{}' is already bound by view [{}]; refusing (fail closed)",
		             geometryName, owner);
		return false;
	}

	Binding binding = MakeBinding(root, hit, srv);

	const size_t hidden = binding.culledOverlays.size();
	Store(view, std::move(binding));
	logger::info("BindViewToGeometry: view [{}] bound to '{}' ({} overlay(s) hidden) srv={}",
	             view, geometryName, hidden, static_cast<void*>(srv));
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

	// Constraint: fail closed on a double bind (see code notes).
	if (const ViewId owner = FindBoundOwner(hit.texture, view)) {
		logger::warn("BindViewToScreenTexture: texture '{}' is already bound by view [{}]; refusing (fail closed)",
		             textureSubstring, owner);
		return false;
	}

	Binding binding = MakeBinding(root, hit, srv);

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
	bool slotRestored = false;
	if (it->second.texture) {
		// Constraint: restore only while the slot still holds our SRV (see code notes).
		auto* const slotNow = Engine::PeekEngineTextureSRV(it->second.texture);
		if (SrvPolicy::DecideRestore(slotNow, it->second.weWrote) == SrvPolicy::Restore::Original) {
			Engine::PokeEngineTextureSRV(it->second.texture, it->second.original.Get());
			slotRestored = true;
		} else {
			logger::info("UnbindViewFromGeometry: view [{}] texture slot was replaced (now={} wrote={}); leaving it alone",
			             view, static_cast<void*>(slotNow), static_cast<void*>(it->second.weWrote));
		}
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
	s_activeBindings.store(s_meshBindings.size(), std::memory_order_release);
	logger::info("UnbindViewFromGeometry: view [{}] released ({} overlay(s) shown, srv slot restored={})",
	             view, restored, slotRestored);
}

}
