#!/usr/bin/env bash
for t in gcc make git bc flex bison; do
  printf '%s: ' "$t"; command -v "$t" || echo MISSING
done
echo "--- libs ---"
for p in libssl-dev libelf-dev dwarves; do
  if dpkg -s "$p" >/dev/null 2>&1; then echo "$p: ok"; else echo "$p: MISSING"; fi
done
