#!/bin/sh
set -e

REPO="chrisomatic/censorman"
INSTALL_DIR="/usr/local/bin"
BINARY="censorman"

# ─────────────────────────────────────────────
# Detect OS and architecture
# ─────────────────────────────────────────────
OS=$(uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)

case "$OS" in
  darwin) OS="darwin" ;;
  linux)  OS="linux" ;;
  *)
    echo "ERROR: Unsupported OS: $OS"
    exit 1
    ;;
esac

case "$ARCH" in
  x86_64)          ARCH="x86_64" ;;
  amd64)           ARCH="x86_64" ;;
  arm64 | aarch64) ARCH="amd64"  ;;  # macOS Apple Silicon
  *)
    echo "ERROR: Unsupported architecture: $ARCH"
    exit 1
    ;;
esac

# ─────────────────────────────────────────────
# Fetch latest release tag from GitHub API
# ─────────────────────────────────────────────
echo "Fetching latest release info..."
TAG=$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" \
  | grep '"tag_name"' \
  | sed 's/.*"tag_name": *"\([^"]*\)".*/\1/')

if [ -z "$TAG" ]; then
  echo "ERROR: Could not determine the latest release tag."
  echo "Make sure curl is available and you have internet access."
  exit 1
fi

echo "Latest release: $TAG"

# ─────────────────────────────────────────────
# Build asset name and download URL
# ─────────────────────────────────────────────
ASSET="censorman-v2-${OS}-${ARCH}.tar.gz"
DOWNLOAD_URL="https://github.com/$REPO/releases/download/$TAG/$ASSET"
TMP_DIR=$(mktemp -d)
TMP_TAR="$TMP_DIR/$ASSET"

echo "Downloading $ASSET..."
curl -fsSL "$DOWNLOAD_URL" -o "$TMP_TAR"

# ─────────────────────────────────────────────
# Extract binary
# ─────────────────────────────────────────────
echo "Extracting..."
tar -xzf "$TMP_TAR" -C "$TMP_DIR"

# ─────────────────────────────────────────────
# Install binary (fallback to ~/bin if no sudo)
# ─────────────────────────────────────────────
if [ -w "$INSTALL_DIR" ]; then
  cp "$TMP_DIR/$BINARY" "$INSTALL_DIR/$BINARY"
  chmod +x "$INSTALL_DIR/$BINARY"
else
  if command -v sudo >/dev/null 2>&1; then
    echo "Requesting sudo to install to $INSTALL_DIR..."
    sudo cp "$TMP_DIR/$BINARY" "$INSTALL_DIR/$BINARY"
    sudo chmod +x "$INSTALL_DIR/$BINARY"
  else
    # Fallback: install to ~/bin
    INSTALL_DIR="$HOME/bin"
    mkdir -p "$INSTALL_DIR"
    cp "$TMP_DIR/$BINARY" "$INSTALL_DIR/$BINARY"
    chmod +x "$INSTALL_DIR/$BINARY"
    echo "NOTE: Installed to $INSTALL_DIR (no sudo available)."
  fi
fi

# ─────────────────────────────────────────────
# Cleanup
# ─────────────────────────────────────────────
rm -rf "$TMP_DIR"

# ─────────────────────────────────────────────
# PATH check
# ─────────────────────────────────────────────
echo ""
echo "censorman installed to $INSTALL_DIR/$BINARY"
echo ""

case ":$PATH:" in
  *":$INSTALL_DIR:"*)
    ;;
  *)
    echo "NOTE: $INSTALL_DIR is not in your PATH."
    echo "Add the following line to your shell config (~/.bashrc, ~/.zshrc, etc.):"
    echo ""
    echo "  export PATH=\"\$PATH:$INSTALL_DIR\""
    echo ""
    echo "Then restart your shell or run: source ~/.bashrc"
    echo ""
    ;;
esac

echo "Done! Run: censorman --help"
