#!/bin/sh
# Azen Sandbox installer.
#   ./install.sh               build (if needed) and install for this user
#   ./install.sh --uninstall   remove the binary, icon and launcher
set -e

DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
BIN="$DIR/build/azen-sandbox"
DEST="$HOME/.local/bin"
ICON_DIR="$HOME/.local/share/icons/hicolor/256x256/apps"
APPS_DIR="$HOME/.local/share/applications"
APP_ID="org.nvazen.azensandbox"

if [ "${1:-}" = "--uninstall" ]; then
    rm -f "$DEST/azen-sandbox" "$ICON_DIR/$APP_ID.png" "$APPS_DIR/$APP_ID.desktop"
    update-desktop-database "$APPS_DIR" 2>/dev/null || true
    echo "Azen Sandbox removed."
    exit 0
fi

echo "==> Azen Sandbox installer"
if [ ! -x "$BIN" ]; then
    echo "==> Building (needs cmake, g++, Qt6 Widgets, X11)"
    cmake -S "$DIR" -B "$DIR/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build "$DIR/build" -j"$(nproc)"
fi

mkdir -p "$DEST" "$ICON_DIR" "$APPS_DIR"
install -m 755 "$BIN" "$DEST/azen-sandbox" 2>/dev/null || { cp "$BIN" "$DEST/azen-sandbox"; chmod 755 "$DEST/azen-sandbox"; }
if [ -f "$DIR/assets/azen-sandbox.png" ]; then
    cp "$DIR/assets/azen-sandbox.png" "$ICON_DIR/$APP_ID.png"
fi

cat > "$APPS_DIR/$APP_ID.desktop" <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Azen Sandbox
GenericName=Toy Desktop
Comment=A tiny toy desktop that runs real apps inside a window
Exec=$DEST/azen-sandbox
Icon=$APP_ID
Terminal=false
Categories=Utility;System;
Keywords=zen;sandbox;desktop;toy;wine;
StartupNotify=true
EOF
chmod +x "$APPS_DIR/$APP_ID.desktop"
update-desktop-database "$APPS_DIR" 2>/dev/null || true

echo "==> Done. Launch 'Azen Sandbox' from the app menu, or run:"
echo "    $DEST/azen-sandbox"
echo "    (Windows apps need wine: sudo pacman -S wine)"
