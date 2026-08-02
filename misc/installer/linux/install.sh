#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$(dirname "$SCRIPT_DIR")")")"
INSTALLER_DIR="$SCRIPT_DIR/arch"

ICON_DIR="$HOME/.local/share/icons/hicolor/512x512/apps"
MIME_ICON_DIR="$HOME/.local/share/icons/hicolor/512x512/mimetypes"
APP_DIR="$HOME/.local/share/applications"
BIN_DIR="$HOME/.local/bin"
MIME_DIR="$HOME/.local/share/mime/packages"

mkdir -p "$ICON_DIR" "$MIME_ICON_DIR" "$APP_DIR" "$BIN_DIR" "$MIME_DIR"

echo "Installing Alkyl executable..."
if [ -f "$PROJECT_ROOT/build/alkyl" ]; then
    install -Dm755 "$PROJECT_ROOT/build/alkyl" "$BIN_DIR/alkyl"
else
    echo "Error: build/alkyl not found. Run cmake build first."
    exit 1
fi

echo "Installing icons..."
if [ -f "$PROJECT_ROOT/misc/asset/logo.png" ]; then
    install -Dm644 "$PROJECT_ROOT/misc/asset/logo.png" "$ICON_DIR/alkyl.png"
fi

if [ -f "$PROJECT_ROOT/misc/asset/script_logo.png" ]; then
    install -Dm644 "$PROJECT_ROOT/misc/asset/script_logo.png" "$MIME_ICON_DIR/application-x-alkyl.png"
fi

echo "Installing desktop entry and MIME types..."
if [ -f "$INSTALLER_DIR/alkyl.desktop" ]; then
    install -Dm644 "$INSTALLER_DIR/alkyl.desktop" "$APP_DIR/alkyl.desktop"
fi

if [ -f "$INSTALLER_DIR/application-x-alkyl.xml" ]; then
    install -Dm644 "$INSTALLER_DIR/application-x-alkyl.xml" "$MIME_DIR/application-x-alkyl.xml"
fi

echo "Updating caches..."
update-desktop-database "$APP_DIR" 2>/dev/null || true
update-mime-database "$HOME/.local/share/mime" 2>/dev/null || true
gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" 2>/dev/null || true

echo "Installation complete!"
echo "Make sure $BIN_DIR is in your PATH."
