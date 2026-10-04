#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$project_root" -B "$project_root/build-macos" -DCMAKE_BUILD_TYPE=Release
cmake --build "$project_root/build-macos" --config Release
mkdir -p "$project_root/dist"
macdeployqt "$project_root/build-macos/SendToCmd.app" -dmg
cp "$project_root/build-macos/SendToCmd.dmg" "$project_root/dist/SendToCmd-macos.dmg"
