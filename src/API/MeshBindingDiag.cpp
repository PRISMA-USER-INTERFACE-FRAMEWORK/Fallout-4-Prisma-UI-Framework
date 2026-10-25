#include "MeshBindingInternal.h"

#include "Engine/EngineInterface3D.h"
#include "Engine/EngineTexture.h"

#include "RE/I/Interface3D.h"

#include <format>

namespace PrismaUI::MeshBinding
{
using namespace Internal;

namespace
{
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

}

namespace Internal
{
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

	void DumpInterface3DRenderer(RE::BSGeometry* boundGeom)
	{
		logger::info("[DIAG-I3D] global renderer list: {} entries",
		             Engine::Interface3DRendererCount());
		Engine::ForEachInterface3DRenderer(
			[boundGeom](RE::Interface3D::Renderer* r) { DumpOneRenderer(r, boundGeom, "global"); });
	}

}

void DiagTick(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> (*currentSrv)(ViewId))
{
	static uint64_t frame = 0;
	const bool probe = (++frame % 120 == 0);
	if (s_activeBindings.load(std::memory_order_acquire) == 0) return;
	std::lock_guard lock(s_meshBindingsMutex);

	const std::uint64_t reClearMask = ScreenClearMask();
	for (auto& [view, b] : s_meshBindings) {
		if (currentSrv && b.texture) {
			if (auto current = currentSrv(view); current && current.Get() != b.srvRef.Get()) {
				Engine::PokeEngineTextureSRV(b.texture, current.Get());
				b.srvRef = current;
				b.weWrote = current.Get();
				logger::info("[MeshBinding] view [{}] re-pointed to the resized offscreen frame srv={}", view,
				             static_cast<void*>(current.Get()));
			}
		}
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
		auto* prop = b.property.get();
		const bool culled = b.geometry && b.geometry->GetAppCulled();
		const char* geomName = b.geometry && b.geometry->name.c_str() ? b.geometry->name.c_str() : "?";
		logger::debug("[DIAG-MESH] view={} geom='{}' culled={} bsTex={} pSRView(now)={} weWrote={} original={} "
		              "STOMPED={} propFlags=0x{:016x} flagStomps={} passState={:#x} alpha={:.3f} lodFade={:.3f} "
		              "matAlpha={:.3f} passList={}",
		              view, geomName, culled, static_cast<void*>(b.texture), static_cast<void*>(now),
		              static_cast<void*>(b.weWrote), static_cast<void*>(b.original.Get()),
		              now != b.weWrote ? "YES" : "no",
		              prop ? prop->flags.underlying() : 0, b.flagStomps, prop ? prop->lastRenderPassState : 0,
		              prop ? prop->alpha : -1.0f, prop ? prop->lodFade : -1.0f, prop ? prop->QMaterialAlpha() : -1.0f,
		              prop ? static_cast<void*>(prop->renderPassList.passList) : nullptr);
	}
}
}
