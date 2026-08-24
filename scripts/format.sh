#!/usr/bin/env bash
# Run the repository's pinned clang-format through Docker.
# Copyright (C) 2026  Max Bodaniuk
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="${CLANG_FORMAT_IMAGE:-esp32-weather-epd-clang-format:18.1.8-alpine}"
CHECK=false

if [[ "${1:-}" == "--check" ]]; then
  CHECK=true
  shift
fi

if [[ "${1:-}" == "--" ]]; then
  shift
fi

if [[ "$#" -eq 0 ]]; then
  files=()
  while IFS= read -r file; do
    files+=("$file")
  done < <(
    git -C "$ROOT" ls-files -- 'src/**' 'include/**' 'test/src/**' \
      | grep -E '\.(c|cc|cpp|cxx|h|hh|hpp|hxx|inc)$' \
      | grep -Ev '(^include/cert\.h$|test/src/.*feed.*\.inc$|test/src/.*real\.inc$)' || true
  )
else
  files=("$@")
fi

if [[ "${#files[@]}" -eq 0 ]]; then
  echo "No C/C++ files to format."
  exit 0
fi

printf 'Using %s\n' "$IMAGE"
docker build --quiet --tag "$IMAGE" "$ROOT/format"

args=(--style=file)
if [[ "$CHECK" == true ]]; then
  args+=(--dry-run --Werror)
else
  args+=(-i)
fi

docker run --rm \
  --user "$(id -u):$(id -g)" \
  --volume "$ROOT:/project" \
  --workdir /project \
  "$IMAGE" \
  /usr/lib/llvm18/bin/clang-format "${args[@]}" "${files[@]}"
