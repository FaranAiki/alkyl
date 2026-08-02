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
LIB_DIR="$HOME/.local/share/alkyl"

mkdir -p "$ICON_DIR" "$MIME_ICON_DIR" "$APP_DIR" "$BIN_DIR" "$MIME_DIR" "$LIB_DIR"

install_binary() {
    local src="$1"
    local dest="$2"
    if [ -f "$src" ]; then
        install -Dm755 "$src" "$dest"
    else
        echo "Warning: $src not found, skipping."
    fi
}

install_desktop() {
    local src="$1"
    local dest="$2"
    if [ -f "$src" ]; then
        install -Dm644 "$src" "$dest"
    fi
}

echo "Installing Alkyl compiler binaries..."
install_binary "$PROJECT_ROOT/build/alkyl" "$BIN_DIR/alkyl"
install_binary "$PROJECT_ROOT/build/alkyl_llvm" "$BIN_DIR/alkyl_llvm"
install_binary "$PROJECT_ROOT/build/alkyl_qbe" "$BIN_DIR/alkyl_qbe"
install_binary "$PROJECT_ROOT/build/alkyl_mlir" "$BIN_DIR/alkyl_mlir"
install_binary "$PROJECT_ROOT/build/alkyl_cranelift" "$BIN_DIR/alkyl_cranelift"
install_binary "$PROJECT_ROOT/build/ethyl" "$BIN_DIR/ethyl"

echo "Installing standard libraries..."
if [ -d "$PROJECT_ROOT/lib" ]; then
    cp -r "$PROJECT_ROOT/lib/." "$LIB_DIR/"
fi

echo "Installing icons..."
install -Dm644 "$PROJECT_ROOT/misc/asset/logo.png" "$ICON_DIR/alkyl.png"
install -Dm644 "$PROJECT_ROOT/misc/asset/script_logo.png" "$MIME_ICON_DIR/application-x-alkyl.png"

echo "Installing desktop entries..."
install_desktop "$INSTALLER_DIR/alkyl.desktop" "$APP_DIR/alkyl.desktop"
install_desktop "$INSTALLER_DIR/alkyl-llvm.desktop" "$APP_DIR/alkyl-llvm.desktop"
install_desktop "$INSTALLER_DIR/alkyl-qbe.desktop" "$APP_DIR/alkyl-qbe.desktop"
install_desktop "$INSTALLER_DIR/alkyl-mlir.desktop" "$APP_DIR/alkyl-mlir.desktop"
install_desktop "$INSTALLER_DIR/alkyl-cranelift.desktop" "$APP_DIR/alkyl-cranelift.desktop"
install_desktop "$INSTALLER_DIR/ethyl.desktop" "$APP_DIR/ethyl.desktop"

echo "Installing MIME types..."
if [ -f "$INSTALLER_DIR/application-x-alkyl.xml" ]; then
    install -Dm644 "$INSTALLER_DIR/application-x-alkyl.xml" "$MIME_DIR/application-x-alkyl.xml"
fi

echo "Updating caches..."
update-desktop-database "$APP_DIR" 2>/dev/null || true
update-mime-database "$HOME/.local/share/mime" 2>/dev/null || true
gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" 2>/dev/null || true

echo "Installation complete!"
echo "Make sure $BIN_DIR is in your PATH."
