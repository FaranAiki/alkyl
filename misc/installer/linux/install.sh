#!/bin/bash

# Resolve the absolute path of the project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

ICON_DIR="$HOME/.local/share/icons/hicolor/512x512/apps"
MIME_ICON_DIR="$HOME/.local/share/icons/hicolor/512x512/mimetypes"
APP_DIR="$HOME/.local/share/applications"
BIN_DIR="$HOME/.local/bin"
MIME_DIR="$HOME/.local/share/mime/packages"

mkdir -p "$ICON_DIR" "$MIME_ICON_DIR" "$APP_DIR" "$BIN_DIR" "$MIME_DIR"

echo "Installing Alkyl executable and desktop icon..."
if [ -f "$PROJECT_ROOT/build/alkyl" ]; then
    cp "$PROJECT_ROOT/build/alkyl" "$BIN_DIR/alkyl"
else
    echo "Warning: build/alkyl not found. Run make/cmake first."
fi

if [ -f "$PROJECT_ROOT/misc/asset/icon.png" ]; then
    cp "$PROJECT_ROOT/misc/asset/icon.png" "$ICON_DIR/alkyl.png"
fi

cat <<EOF > "$APP_DIR/alkyl.desktop"
[Desktop Entry]
Name=Alkyl Compiler
Exec=$BIN_DIR/alkyl %F
Icon=alkyl
Terminal=true
Type=Application
Categories=Development;
EOF
update-desktop-database "$APP_DIR" 2>/dev/null

echo "Installing Alkyl MIME types and file extension icons..."
cat <<EOF > "$MIME_DIR/application-x-alkyl.xml"
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
  <mime-type type="application/x-alkyl">
    <comment>Alkyl Source File</comment>
    <glob pattern="*.aky"/>
    <glob pattern="*.hky"/>
    <icon name="application-x-alkyl"/>
  </mime-type>
</mime-info>
EOF

if [ -f "$PROJECT_ROOT/misc/asset/script_logo.png" ]; then
    cp "$PROJECT_ROOT/misc/asset/script_logo.png" "$MIME_ICON_DIR/application-x-alkyl.png"
fi

update-mime-database "$HOME/.local/share/mime" 2>/dev/null
gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" 2>/dev/null

echo "Installation complete!"
