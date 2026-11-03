import concurrent.futures
import os
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TESTS = (
    ("core_security_tests", "-Isrc", "tests/core_security_tests.cpp", "tests/core_security_tests_url.cpp"),
    ("logical_path_policy_tests", "-Isrc", "tests/logical_path_policy_tests.cpp"),
    ("translations_text_tests", "-Isrc", "tests/translations_text_tests.cpp", "src/PrismaUI/TranslationsText.cpp"),
    ("translations_json_tests", "-Isrc", "tests/translations_json_tests.cpp", "src/PrismaUI/TranslationsJson.cpp"),
    ("translations_language_tests", "-Isrc", "tests/translations_language_tests.cpp", "src/PrismaUI/TranslationsLanguage.cpp"),
    ("translations_v3_script_test", "-Isrc", "tests/translations_v3_script_test.cpp", "src/PrismaUI/TranslationsScript.cpp"),
    ("gpu_generation_drain_tests", "-Isrc", "-Iexternal/ultralight-1.4.0/include", "tests/gpu_generation_drain_tests.cpp", "src/GPU/GPUDriverBase.cpp"),
    ("gpu_pipeline_warp_tests", "-Isrc", "-Iexternal/ultralight-1.4.0/include", "tests/gpu_pipeline_warp_tests.cpp", "-ld3d11", "-ld3dcompiler"),
    ("external_texture_registry_tests", "-pthread", "-Isrc", "tests/external_texture_registry_tests.cpp", "-ld3d11"),
    ("imagesource_token_tests", "-pthread", "-Isrc", "tests/imagesource_token_tests.cpp"),
    ("modelpreview_imagesource_policy_tests", "-Isrc", "tests/modelpreview_imagesource_policy_tests.cpp"),
    ("large_texture_transport_policy_test", "-Isrc", "-Iexternal/ultralight-1.4.0/include", "tests/large_texture_transport_policy_test.cpp"),
    ("accelerated_proxy_ownership_tests", "-Isrc", "tests/accelerated_proxy_ownership_tests.cpp"),
    ("network_sandbox_script_tests", "-Isrc", "tests/network_sandbox_script_tests.cpp"),
    ("shell_call_format_tests", "-Isrc", "tests/shell_call_format_tests.cpp"),
    ("borrowed_srv_policy_tests", "-Isrc", "tests/borrowed_srv_policy_tests.cpp"),
    ("mesh_binding_srv_policy_tests", "-Isrc", "tests/mesh_binding_srv_policy_tests.cpp"),
    ("mesh_binding_readiness_policy_tests", "-Isrc", "tests/mesh_binding_readiness_policy_tests.cpp"),
    ("pending_bind_intake_order_tests", "-Isrc", "tests/pending_bind_intake_order_tests.cpp"),
    ("interface3d_offscreen_slot_policy_tests", "-Isrc", "tests/interface3d_offscreen_slot_policy_tests.cpp"),
    ("scene_depth_draw_policy_tests", "-Isrc", "tests/scene_depth_draw_policy_tests.cpp"),
    ("offscreen_frame_state_tests", "-Isrc", "tests/offscreen_frame_state_tests.cpp"),
    ("hud_address_lookup_policy_tests", "-Isrc", "tests/hud_address_lookup_policy_tests.cpp"),
    ("engine_function_body_scan_tests", "-Isrc", "tests/engine_function_body_scan_tests.cpp"),
    ("engine_re_layout_tests", "-I../../shared/CommonLibF4/include", "-I../../shared/CommonLibF4/lib/commonlib-shared/include", "tests/engine_re_layout_tests.cpp"),
    ("backbuffer_ownership_tests", "tests/backbuffer_ownership_tests.cpp"),
    ("inline_hook_classifier_test", "-Isrc", "tests/inline_hook_classifier_test.cpp", "src/Hooks/InlineHookClassifier.cpp"),
    ("reattach_policy_test", "-Isrc", "tests/reattach_policy_test.cpp", "src/Hooks/ReattachPolicy.cpp"),
    ("present_stability_test", "-Isrc", "tests/present_stability_test.cpp", "src/Hooks/ReattachPolicy.cpp"),
    ("refcount_proof", "tests/refcount_proof.cpp"),
    ("context_lock_budget_test", "-pthread", "tests/context_lock_budget_test.cpp"),
    ("game_thread_dispatcher_test", "-pthread", "-Isrc", "tests/game_thread_dispatcher_test.cpp"),
    ("dispatcher_recovery_test", "-pthread", "-Itests/dispatcher_stub", "-Isrc", "tests/dispatcher_recovery_test.cpp", "src/PrismaUI/GameThreadDispatcher.cpp", "-luser32"),
    ("health_failsafe_test", "tests/health_failsafe_test.cpp"),
    ("view_network_policy_tests", "-Isrc", "tests/view_network_policy_tests.cpp", "src/Engine/ViewNetworkPolicy.cpp"),
    ("papyrus_json_tests", "-Isrc", "tests/papyrus_json_tests.cpp"),
    ("freeze_diagnostics_test", "-Isrc", "tests/freeze_diagnostics_test.cpp", "src/PrismaUI/FreezeDiagnosticsCore.cpp"),
    ("thread_observer_tests", "-pthread", "-Isrc", "tests/thread_observer_tests.cpp"),
    ("present_hold_policy_test", "-Isrc", "tests/present_hold_policy_test.cpp"),
    ("late_rerender_policy_test", "-Isrc", "tests/late_rerender_policy_test.cpp"),
    ("view_cursor_policy_test", "-Isrc", "tests/view_cursor_policy_test.cpp"),
    ("presentation_scale_policy_test", "-Isrc", "tests/presentation_scale_policy_test.cpp"),
    ("focus_cursor_sample_policy_test", "-Isrc", "tests/focus_cursor_sample_policy_test.cpp"),

    ("focus_cursor_reassert_policy_test", "-Isrc", "tests/focus_cursor_reassert_policy_test.cpp"),
    ("runtime_startup_test", "-pthread", "-Isrc", "tests/runtime_startup_test.cpp"),
    ("ultralight_devtools_policy_test", "-Isrc", "tests/ultralight_devtools_policy_test.cpp"),
    ("presentation_heartbeat_policy_test", "-Isrc", "tests/presentation_heartbeat_policy_test.cpp"),
    ("controller_key_state_tests", "-Isrc", "tests/controller_key_state_tests.cpp"),
    ("native_gamepad_tests", "-Isrc", "tests/native_gamepad_tests.cpp"),
    ("native_gamepad_publish_tests", "-Isrc", "-Iexternal/ultralight-1.4.0/include", "tests/native_gamepad_publish_tests.cpp"),
    ("native_menu_hook_install_tests", "-Isrc", "tests/native_menu_hook_install_tests.cpp"),
    ("controller_menu_key_tests", "-Isrc", "-I../../shared/CommonLibF4/include", "tests/controller_menu_key_tests.cpp"),
    ("plugin_runtime_policy_tests", "-Isrc", "tests/plugin_runtime_policy_tests.cpp"),
    ("controller_abi_epoch_tests", "-Isrc", "tests/controller_abi_epoch_tests.cpp"),
    ("virtual_pointer_policy_tests", "-Isrc", "tests/virtual_pointer_policy_tests.cpp"),
    ("virtual_pointer_state_tests", "-Isrc", "tests/virtual_pointer_state_tests.cpp"),
    ("webinput_window_state_tests", "-pthread", "-Isrc", "tests/webinput_window_state_tests.cpp"),
    ("controller_action_routing_tests", "-pthread", "-Isrc", "tests/controller_action_routing_tests.cpp", "src/API/ControllerActionPolicy.cpp"),
    ("controller_focus_entry_policy_tests", "-pthread", "-Isrc", "tests/controller_focus_entry_policy_tests.cpp"),
    ("controller_focus_entry_registry_tests", "-pthread", "-Itests/focus_entry_stubs", "-Isrc", "tests/controller_focus_entry_registry_tests.cpp", "src/API/ControllerActions.cpp", "src/API/ControllerActionPolicy.cpp"),
    ("controller_focus_churn_tests", "-pthread", "-Isrc", "tests/controller_focus_churn_tests.cpp", "src/API/ControllerActionPolicy.cpp"),
    ("controller_event_storm_stress_tests", "-pthread", "-Isrc", "tests/controller_event_storm_stress_tests.cpp", "src/API/ControllerActionPolicy.cpp"),
    ("flat_api_host_tests", "-Isrc", "-Itests", "-DPRISMAUI_FLAT_API_HOST_TEST", "tests/flat_api_host_tests.cpp", "src/API/FlatAPI.cpp"),
    ("render_diagnostics_tests", "-Isrc", "tests/render_diagnostics_tests.cpp"),
    ("adapter_luid_policy_tests", "-Isrc", "tests/adapter_luid_policy_tests.cpp"),
    ("engine_texture_layout_support_tests", "-Isrc", "tests/engine_texture_layout_support_tests.cpp"),
    ("engine_vr_address_library_tests", "-Isrc", "tests/engine_vr_address_library_tests.cpp"),

    ("clear_color_policy_tests", "-Isrc", "tests/clear_color_policy_tests.cpp"),
    ("partial_upload_policy_tests", "-Isrc", "tests/partial_upload_policy_tests.cpp"),
    ("script_completion_tests", "-pthread", "-Itests/dispatcher_stub", "-Isrc", "tests/script_completion_tests.cpp", "src/PrismaUI/UltralightDispatchQueue.cpp"),
    ("queue_latency_stats_test", "-Isrc", "tests/queue_latency_stats_test.cpp"),
    ("texture_overlay_geometry_tests", "-Isrc", "tests/texture_overlay_geometry_tests.cpp"),

    ("input_region_policy_tests", "-Isrc", "tests/input_region_policy_tests.cpp", "src/PrismaUI/InputRegionPolicy.cpp"),
    ("ime_focus_policy_tests", "-Isrc", "tests/ime_focus_policy_tests.cpp"),
    ("utf16_pairing_tests", "-Isrc", "tests/utf16_pairing_tests.cpp"),
    ("modelpreview_shutdown_gate_tests", "-pthread", "-Isrc", "tests/modelpreview_shutdown_gate_tests.cpp"),
)
SYNTAX = (
    ("ultralight_svg_raster_acceptance", "-Iexternal/ultralight-1.4.0/include", "tests/ultralight_svg_raster_acceptance.cpp"),
    ("ultralight_gamepad_acceptance", "-Isrc", "-Iexternal/ultralight-1.4.0/include", "tests/ultralight_gamepad_acceptance.cpp"),
)


def run(*args: object, env: dict[str, str], stdout=None) -> None:
    subprocess.run([str(arg) for arg in args], cwd=ROOT, env=env, stdout=stdout, check=True)


def capture(*args: object, env: dict[str, str]) -> tuple[int, str]:
    result = subprocess.run([str(arg) for arg in args], cwd=ROOT, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace")
    return result.returncode, result.stdout


def build_and_run(compiler: Path, flags: tuple[str, ...], temp: str, env: dict[str, str],
                  name: str, sources: list[str]) -> tuple[int, str]:
    output = Path(temp) / f"{name}.exe"
    sanitizer = ("-fsanitize=address,undefined",)
    if name == "gpu_generation_drain_tests":
        sanitizer += ("-fno-sanitize=alignment", "-DULTRALIGHT_IMPLEMENTATION")
    code, log = capture(compiler, *flags, *sanitizer, *sources, "-o", output, env=env)
    if code:
        return code, log
    code, test_log = capture(output, env=env)
    return code, log + test_log


def main() -> None:
    sys.stdout.reconfigure(errors="replace")
    if os.name != "nt":
        raise SystemExit("Windows-only test gate")
    llvm = Path(os.environ.get("PRISMA_LLVM_BIN", r"C:\Program Files\LLVM\bin"))
    compiler = llvm / "clang++.exe"
    if not compiler.is_file():
        raise FileNotFoundError(compiler)
    resource = Path(subprocess.check_output([str(compiler), "-print-resource-dir"], text=True).strip())
    runtime = resource / "lib/windows"
    env = os.environ.copy()
    env["PATH"] = f"{runtime};{llvm};{env['PATH']}"
    env["CXX"] = str(compiler)
    env["PRISMA_REFERENCE_GUARD_USE_SNAPSHOT"] = "1"
    static_guards = ROOT / "tests/static_guards.py"
    if static_guards.is_file() and any(line.startswith(("<<<<<<<", "=======", ">>>>>>>")) for line in
                                       static_guards.read_text(encoding="utf-8").splitlines()):
        raise ValueError("static_guards.py has merge conflict markers")
    for guard in sorted((ROOT / "tests").glob("*.py")):
        if guard.name != Path(__file__).name:
            print(f"== {guard.name} ==", flush=True)
            run("python", guard, env=env)
    flags = ("-std=c++23", "-O1", "-g", "-Wall", "-Wextra", "-Wpedantic")
    jobs = [(name, sources) for name, *sources in TESTS
            if (ROOT / "tests" / f"{name}.cpp").is_file() or name == "translations_v3_script_test"]
    with tempfile.TemporaryDirectory(prefix="prisma-windows-gate-") as temp:
        pool = concurrent.futures.ThreadPoolExecutor(max_workers=min(os.cpu_count() or 1, 8))
        try:
            futures = {pool.submit(build_and_run, compiler, flags, temp, env, name, sources): name
                       for name, sources in jobs}
            for future in concurrent.futures.as_completed(futures):
                name = futures[future]
                code, log = future.result()
                if code:
                    raise RuntimeError(f"{name} failed with exit code {code}\n{log}")
                print(f"== {name} ==\n{log}", flush=True)
        finally:
            pool.shutdown(wait=True, cancel_futures=True)
        emitter = ROOT / "tests/translations_v4_script_emitter.cpp"
        if emitter.is_file():
            output = Path(temp) / "translations_v4_script_emitter.exe"
            script = Path(temp) / "translations_v4_emitted.js"
            run(compiler, *flags, "-fsanitize=address,undefined", "-Isrc", emitter,
                "src/PrismaUI/TranslationsScript.cpp", "-o", output, env=env)
            with script.open("wb") as stream:
                run(output, env=env, stdout=stream)
            run("node", "tests/translations_v4_behavior.js", script, env=env)
        for name, *sources in SYNTAX:
            if (ROOT / "tests" / f"{name}.cpp").is_file():
                print(f"== {name} syntax ==", flush=True)
                run(compiler, *flags, "-fsyntax-only", *sources, env=env)
    print("Windows static/unit gate passed")


if __name__ == "__main__":
    main()
