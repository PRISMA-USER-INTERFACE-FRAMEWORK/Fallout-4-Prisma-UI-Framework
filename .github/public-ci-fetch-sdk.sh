#!/usr/bin/env bash
# Copies the vendored Ultralight SDK from the private Prisma-Matrix commit named in this commit's
# Source-Commit trailer into $SDK_PATH. Reads only that one folder; nothing is uploaded or cached.
set -euo pipefail
: "${TOKEN:?PUBLIC_CI_TOKEN is not available to this run}"
: "${PRIVATE_REPO:?}" "${SDK_PATH:?}"
source=$(git log -1 --format=%B | sed -n 's/^Source-Commit: \([0-9a-f]\{40\}\)$/\1/p')
[ -n "$source" ] || { echo "::error::no Source-Commit trailer"; exit 1; }
echo "::add-mask::$TOKEN"
work=$(mktemp -d)
auth=$(printf 'x-access-token:%s' "$TOKEN" | base64 | tr -d '\n')
git -C "$work" init -q
git -C "$work" remote add origin "https://github.com/$PRIVATE_REPO.git"
git -C "$work" -c "http.extraheader=AUTHORIZATION: basic $auth" fetch -q --depth 1 --filter=blob:none origin "$source"
git -C "$work" sparse-checkout set --cone "$SDK_PATH"
git -C "$work" -c "http.extraheader=AUTHORIZATION: basic $auth" checkout -q FETCH_HEAD
[ -d "$work/$SDK_PATH/include/Ultralight" ] && [ -f "$work/$SDK_PATH/lib/Ultralight.lib" ] ||
    { echo "::error::SDK not found at $SDK_PATH in $source"; exit 1; }
mkdir -p "$(dirname "$SDK_PATH")"
rm -rf "$SDK_PATH"
cp -R "$work/$SDK_PATH" "$SDK_PATH"
rm -rf "$work"
echo "Ultralight SDK copied from $PRIVATE_REPO@${source:0:8}"
