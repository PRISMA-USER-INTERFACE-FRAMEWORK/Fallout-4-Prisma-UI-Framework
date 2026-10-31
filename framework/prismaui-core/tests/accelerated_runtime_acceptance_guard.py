from pathlib import Path

from pathlib import Path

backend = Path("src/PrismaUI/UltralightBackend.cpp").read_text(encoding="utf-8")
xmake = Path("xmake.lua").read_text(encoding="utf-8")

assert 'option("prisma_accelerated_runtime_acceptance")' in xmake
assert 'set_default(false)' in xmake
assert 'PRISMA_ACCELERATED_RUNTIME_ACCEPTANCE=1' in xmake
assert 'PRISMA_ACCELERATED_RUNTIME_ACCEPTANCE=0' in xmake
assert "const bool allowAccelerated = ::GetPrivateProfileIntW(" in backend
assert 'L"GPU", L"bAcceleratedRendering", 1, Utils::PluginIniPath()' in backend
assert "PRISMA_ACCELERATED_RUNTIME_ACCEPTANCE != 0" in backend
assert "const bool runtimeAcceptanceBuild = PRISMA_ACCELERATED_RUNTIME_ACCEPTANCE != 0;" in backend
assert "gpuContext_.Initialize(device_.Get(), immediateContext_.Get(), true)" in backend
assert "accelerated_ = allowAccelerated && device_" in backend
assert 'GetPrivateProfileIntW(L"GPU", L"bAccelerated"' not in backend
assert 'GetPrivateProfileIntW(L"GPU", L"bAllowProxiedDevice"' not in backend
assert "bAccelerated=1" not in backend

print("accelerated runtime acceptance gate passed")
