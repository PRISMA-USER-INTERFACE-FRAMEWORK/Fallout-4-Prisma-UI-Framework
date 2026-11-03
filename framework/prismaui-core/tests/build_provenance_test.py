import importlib.util
import json
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("build_provenance", ROOT / "scripts/build-provenance.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    flat, vr = root / "flat", root / "vr"
    flat.mkdir()
    vr.mkdir()
    for directory, stem in ((flat, "PrismaUI_F4"), (vr, "PrismaUI_F4VR")):
        for suffix in (".dll", ".pdb"):
            (directory / (stem + suffix)).write_bytes((stem + suffix).encode())
    acceptance = flat / "PrismaLocalizationAcceptance.dll"
    acceptance.write_bytes(b"PrismaLocalizationAcceptance.dll")
    controller_test = flat / "PrismaControllerTest_F4.dll"
    controller_test.write_bytes(b"PrismaControllerTest_F4.dll")
    version, commit = "2.1.0", "a" * 40
    record = module.measure(flat, vr, version, commit)
    manifest = flat / "BUILD-PROVENANCE.json"
    manifest.write_text(json.dumps(record))
    module.verify(flat, vr, version, commit)
    for bad_version, bad_commit in (("2.1.1", commit), (version, "b" * 40), (version, "short")):
        try:
            module.verify(flat, vr, bad_version, bad_commit)
        except ValueError:
            pass
        else:
            raise AssertionError("mismatched build identity accepted")
    for directory, stem in ((flat, "PrismaUI_F4"), (vr, "PrismaUI_F4VR")):
        for suffix in (".dll", ".pdb"):
            path = directory / (stem + suffix)
            original = path.read_bytes()
            path.write_bytes(b"stale build")
            try:
                module.verify(flat, vr, version, commit)
            except ValueError:
                pass
            else:
                raise AssertionError(f"changed build bytes accepted: {path.name}")
            path.write_bytes(original)
    original_acceptance = acceptance.read_bytes()
    acceptance.write_bytes(b"stale acceptance")
    try:
        module.verify(flat, vr, version, commit)
    except ValueError:
        pass
    else:
        raise AssertionError("changed acceptance DLL bytes accepted")
    acceptance.unlink()
    try:
        module.verify(flat, vr, version, commit)
    except FileNotFoundError:
        pass
    else:
        raise AssertionError("build without the acceptance DLL accepted")
    acceptance.write_bytes(original_acceptance)
    controller_test.write_bytes(b"stale controller test")
    try:
        module.verify(flat, vr, version, commit)
    except ValueError:
        pass
    else:
        raise AssertionError("changed controller test DLL bytes accepted")
    controller_test.unlink()
    try:
        module.verify(flat, vr, version, commit)
    except FileNotFoundError:
        pass
    else:
        raise AssertionError("build without the controller test DLL accepted")
    controller_test.write_bytes(b"PrismaControllerTest_F4.dll")
    module.verify(flat, vr, version, commit)
    manifest.unlink()
    try:
        module.verify(flat, vr, version, commit)
    except FileNotFoundError:
        pass
    else:
        raise AssertionError("unattested build accepted")

package = (ROOT / "scripts/ci-package-release.py").read_text(encoding="utf-8")
normalizer = (ROOT.parents[1] / ".github/scripts/normalize-windows-worktree.ps1").read_text(encoding="utf-8")
assert package.index('build-provenance.py", "verify"') < package.index('output.mkdir(')
assert 'flat / "PrismaLocalizationAcceptance.dll"' in package
assert package.index('flat / "PrismaLocalizationAcceptance.dll"') < package.index('output.mkdir(')
assert 'flat / "PrismaLocalizationAcceptance.dll"' in package.rsplit("hashlib.sha256", 1)[0].rsplit("for path in", 1)[1]
assert 'flat / "PrismaControllerTest_F4.dll"' in package
assert package.index('flat / "PrismaControllerTest_F4.dll"') < package.index('output.mkdir(')
assert 'config --local core.filemode false' in normalizer
# The stat cache must be dropped rather than refreshed: after checkout-index rewrites CRLF files to
# LF they shrink, and a bare `update-index --refresh` reports every one as "needs update" and exits
# 128. read-tree clears the cached stat data so the dirty check re-stats the worktree.
assert 'read-tree HEAD' in normalizer
assert 'safe.directory=$submoduleDirectory' in normalizer
# No executable line may call update-index; the flag is only named in an explanatory comment.
assert not [line for line in normalizer.splitlines() if line.lstrip().startswith('git') and 'update-index' in line]
# checkout-index restores files only, so a persistent runner's stale NESTED submodule checkout
# (shared/CommonLibF4's lib/commonlib-shared pinned at an older commit) survives it and reads back
# as a real gitlink change. The re-pin has to run before the per-submodule normalize.
assert 'submodule update --init --recursive' in normalizer
assert normalizer.index('submodule update --init --recursive') < normalizer.index('$ordered = @($submodulePaths')
# Normalizing parent-then-child leaves the parent's gitlink reading dirty while the child still has
# CRLF files, so the walk must run deepest-first.
assert 'Sort-Object' in normalizer and '-Descending' in normalizer
# Every stat-based dirty query here mis-reports a just-normalized file whose blob is byte-identical,
# so the verification must ask for content (diff --quiet, diff-index --cached --quiet) rather than
# for a name list, and must not fall back to a recursive `git status`.
assert 'diff --quiet' in normalizer
assert 'diff-index --cached --quiet HEAD' in normalizer
# No executable line may shell out to the stat-based name-list form; the comments name it to explain
# why it is avoided.
assert not [line for line in normalizer.splitlines()
            if line.lstrip().startswith('git') and 'status --porcelain' in line]
# A parent reports " M <child>" with identical SHAs on both sides (:160000 160000 ...) while the
# child still has CRLF files, so the parent's own content check must exclude its gitlinks. The
# children are verified in their own iterations, and their placement by `submodule status`, which
# compares the recorded gitlink to the child HEAD without consulting the stat cache.
assert '--ignore-submodules=all' in normalizer
assert 'submodule status --recursive' in normalizer
# checkout-index only rewrites an entry whose cached stat differs from disk. After actions/checkout
# writes CRLF and refreshes the index against it, every entry looks up to date and checkout-index is
# a no-op -- which is exactly why the root pass worked and the per-submodule pass did not. The
# submodule loop has to delete its tracked files first, like the root pass does, so the rewrite is
# unconditional. Asserted on the submodule loop specifically, not just anywhere in the file.
submodule_loop = normalizer[normalizer.index('foreach ($submodulePath in $ordered)'):]
delete_index = submodule_loop.index('ls-files)')
checkout_index = submodule_loop.index('checkout-index --all --force')
assert delete_index < checkout_index, "submodule loop must delete tracked files before checkout-index"
assert 'Remove-Item -LiteralPath $submoduleFilePath' in submodule_loop

print("build provenance tests passed")
