[CmdletBinding()]
param(
    [switch]$NormalizeSubmodules
)

$ErrorActionPreference = 'Stop'

$workspace = $env:GITHUB_WORKSPACE
if (-not $workspace -or -not (Test-Path -LiteralPath (Join-Path $workspace '.git'))) {
    throw "GITHUB_WORKSPACE is not a Git checkout"
}

git -c safe.directory=$workspace -C $workspace config --local core.autocrlf false
if ($LASTEXITCODE -ne 0) { throw "git config core.autocrlf failed with exit code $LASTEXITCODE" }
git -c safe.directory=$workspace -C $workspace config --local core.eol lf
if ($LASTEXITCODE -ne 0) { throw "git config core.eol failed with exit code $LASTEXITCODE" }
git -c safe.directory=$workspace -C $workspace config --local core.filemode false
if ($LASTEXITCODE -ne 0) { throw "git config core.filemode failed with exit code $LASTEXITCODE" }

$trackedFiles = @(git -c safe.directory=$workspace -C $workspace ls-files)
if ($LASTEXITCODE -ne 0) { throw "git ls-files failed with exit code $LASTEXITCODE" }
foreach ($trackedFile in $trackedFiles) {
    $trackedPath = Join-Path $workspace $trackedFile
    if (Test-Path -LiteralPath $trackedPath -PathType Leaf) {
        Remove-Item -LiteralPath $trackedPath -Force
    }
}

git -c safe.directory=$workspace -C $workspace checkout-index --all --force
if ($LASTEXITCODE -ne 0) { throw "git checkout-index with LF normalization failed with exit code $LASTEXITCODE" }

# checkout-index rewrites the worktree to the repository bytes while the index still carries the
# pre-checkout stat cache. On a Windows runner core.autocrlf=true the CRLF files shrink when they
# become LF, so the cached size no longer matches and Git believes every file changed. A bare
# `update-index --refresh` prints "needs update" for each one and exits 128, and afterwards the racy
# entries still make `status --porcelain` and a bare `diff-index` report the same files as modified
# with an identical blob on both sides. read-tree drops the cached stat data so the next query
# re-stats the worktree, which is the only form that is stable here.
git -c safe.directory=$workspace -C $workspace read-tree HEAD
if ($LASTEXITCODE -ne 0) { throw "git read-tree after checkout normalization failed with exit code $LASTEXITCODE" }

if ($NormalizeSubmodules) {
    # checkout-index only restores file entries; it never moves a submodule to the commit its parent
    # records. On a persistent runner a stale nested checkout (shared/CommonLibF4's own
    # lib/commonlib-shared pinned at an older commit) therefore survives normalization and shows up
    # as " M lib/commonlib-shared" from inside the parent -- a real gitlink mismatch, not a stat
    # artifact, which read-tree cannot clear either. Re-pin every nested submodule first, so the
    # tree reflects the recorded commit rather than whatever was left behind.
    git -c safe.directory=$workspace -C $workspace submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { throw "recursive submodule re-pin failed with exit code $LASTEXITCODE" }

    # Normalize deepest-first. A parent's gitlink entry is reported dirty while its child still has
    # unnormalized (CRLF) files, so normalizing parent-then-child leaves the parent looking modified
    # when the verification pass reads it. Sorting by descending path depth puts every child ahead of
    # its parent, and clearing them bottom-up leaves each parent's re-stat of the gitlink clean.
    $submodulePaths = @(git -c safe.directory=$workspace -C $workspace submodule foreach --quiet --recursive 'echo $displaypath' 2>$null)
    if ($LASTEXITCODE -ne 0) { throw "git submodule path enumeration failed with exit code $LASTEXITCODE" }
    $ordered = @($submodulePaths | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Sort-Object -Property @{ Expression = { ($_ -split '/').Count } } -Descending)
    foreach ($submodulePath in $ordered) {
        $submoduleDirectory = Join-Path $workspace ($submodulePath -replace '/', '\')
        git -c safe.directory=$submoduleDirectory -C $submoduleDirectory config --local core.autocrlf false
        if ($LASTEXITCODE -ne 0) { throw "git config core.autocrlf failed in $submodulePath with exit code $LASTEXITCODE" }
        git -c safe.directory=$submoduleDirectory -C $submoduleDirectory config --local core.eol lf
        if ($LASTEXITCODE -ne 0) { throw "git config core.eol failed in $submodulePath with exit code $LASTEXITCODE" }
        git -c safe.directory=$submoduleDirectory -C $submoduleDirectory config --local core.filemode false
        if ($LASTEXITCODE -ne 0) { throw "git config core.filemode failed in $submodulePath with exit code $LASTEXITCODE" }

        # Delete the tracked files before checkout-index, exactly as the root pass above does.
        # checkout-index --all only rewrites an entry whose cached stat differs from disk; once
        # `actions/checkout` has written CRLF and Git's index was refreshed against it, every entry
        # looks up to date and checkout-index leaves the CRLF in place. That is why the root pass
        # works and this one did not: the same command, without the delete, is a no-op. Deleting
        # first makes the rewrite unconditional.
        $submoduleFiles = @(git -c safe.directory=$submoduleDirectory -C $submoduleDirectory ls-files)
        if ($LASTEXITCODE -ne 0) { throw "git ls-files failed in $submodulePath with exit code $LASTEXITCODE" }
        foreach ($submoduleFile in $submoduleFiles) {
            $submoduleFilePath = Join-Path $submoduleDirectory $submoduleFile
            if (Test-Path -LiteralPath $submoduleFilePath -PathType Leaf) {
                Remove-Item -LiteralPath $submoduleFilePath -Force
            }
        }

        git -c safe.directory=$submoduleDirectory -C $submoduleDirectory checkout-index --all --force
        if ($LASTEXITCODE -ne 0) { throw "git checkout-index with LF normalization failed in $submodulePath with exit code $LASTEXITCODE" }
        git -c safe.directory=$submoduleDirectory -C $submoduleDirectory read-tree HEAD
        if ($LASTEXITCODE -ne 0) { throw "git read-tree failed in $submodulePath with exit code $LASTEXITCODE" }
    }

    # Parents last, and only now: their gitlink entries can only read clean once the children they
    # point at are themselves clean.
    git -c safe.directory=$workspace -C $workspace read-tree HEAD
    if ($LASTEXITCODE -ne 0) { throw "git read-tree after submodule normalization failed with exit code $LASTEXITCODE" }
}

$changes = @(git -c safe.directory=$workspace -C $workspace diff --ignore-submodules=all --name-only)
if ($LASTEXITCODE -ne 0) { throw "git diff after checkout normalization failed with exit code $LASTEXITCODE" }
if ($changes.Count -ne 0) {
    throw "checkout normalization left tracked changes: $($changes -join ', ')"
}

# diff-index consults stat data and reports a racy entry that diff above already cleared, so only
# its --quiet content form is usable; --ignore-submodules keeps the gitlinks out of the answer.
git -c safe.directory=$workspace -C $workspace diff-index --cached --quiet HEAD
if ($LASTEXITCODE -ne 0) { throw "git index differs from HEAD after checkout normalization" }

if ($NormalizeSubmodules) {
    # Verify per submodule instead of one recursive `git status`. Every stat-based query on this
    # host mis-reports a just-normalized file (CRLF shrank to LF) as modified even when the blob is
    # byte-identical: `status --porcelain` and `diff-files --name-only` both name files whose
    # hash-object equals the index entry. `git diff --quiet` answers from content instead of stat,
    # and `diff-index --cached --quiet HEAD` compares the index to HEAD, so together they cannot be
    # fooled by the racy cache. Untracked files are asked for separately.
    foreach ($submodulePath in $ordered) {
        $submoduleDirectory = Join-Path $workspace ($submodulePath -replace '/', '\')
        # --ignore-submodules=all: the gitlink entries are this module's children, not its own
        # content. Leaving them in makes the parent report " M lib/commonlib-shared" with identical
        # SHAs on both sides (:160000 160000 dfd28a41 dfd28a41), the same stat-cache false positive
        # one level up. Each child is verified in its own iteration below.
        git -c safe.directory=$submoduleDirectory -C $submoduleDirectory diff --quiet --ignore-submodules=all
        if ($LASTEXITCODE -ne 0) { throw "worktree content differs from the index in ${submodulePath}" }
        git -c safe.directory=$submoduleDirectory -C $submoduleDirectory diff-index --cached --quiet HEAD --ignore-submodules=all
        if ($LASTEXITCODE -ne 0) { throw "index content differs from HEAD in ${submodulePath}" }
        $extra = @(git -c safe.directory=$submoduleDirectory -C $submoduleDirectory ls-files --others --exclude-standard)
        if ($LASTEXITCODE -ne 0) { throw "git ls-files --others failed in $submodulePath with exit code $LASTEXITCODE" }
        $untracked = @($extra | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
        if ($untracked.Count -ne 0) {
            throw "submodule normalization left untracked files in ${submodulePath}: $($untracked -join ', ')"
        }
    }

    # A child's commit is checked by `submodule status`, which compares the recorded gitlink to the
    # child's HEAD and does not consult the stat cache. Verified with a leading-space (clean) line;
    # '+' means wrong commit, '-' means not initialized, 'U' means conflict.
    git -c safe.directory=$workspace -C $workspace submodule status --recursive
    if ($LASTEXITCODE -ne 0) { throw "git submodule status failed with exit code $LASTEXITCODE" }
    $placed = @(git -c safe.directory=$workspace -C $workspace submodule status --recursive |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and $_ -notmatch '^ ' })
    if ($placed.Count -ne 0) {
        throw "submodule normalization left mis-pinned submodules: $($placed -join '; ')"
    }
}

Write-Output "Windows worktree normalized to repository bytes"
