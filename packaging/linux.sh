#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$project_root" -B "$project_root/build-linux" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$project_root/build-linux"
appdir="$project_root/dist/SendToCmd.AppDir"
rm -rf "$appdir"
DESTDIR="$appdir" cmake --install "$project_root/build-linux"
mkdir -p "$appdir/usr/share/applications" "$appdir/usr/share/icons/hicolor/scalable/apps"
cp "$project_root/resources/SendToCmd.desktop" "$appdir/usr/share/applications/"
cp "$project_root/resources/sendtocmd.svg" "$appdir/usr/share/icons/hicolor/scalable/apps/"
cd "$project_root/dist"
linuxdeployqt "$appdir/usr/share/applications/SendToCmd.desktop" -appimage
