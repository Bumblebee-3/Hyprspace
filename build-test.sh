#!/bin/bash
# Build and test script for Hyprspace with Hyprland 0.52.0 compatibility fixes

set -e

echo "=========================================="
echo "Hyprspace Build & Test Script"
echo "Hyprland 0.52.0 Compatibility"
echo "=========================================="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if we're in the right directory
if [ ! -f "hyprpm.toml" ]; then
    echo -e "${RED}Error: hyprpm.toml not found. Are you in the Hyprspace directory?${NC}"
    exit 1
fi

echo -e "${YELLOW}Step 1: Checking Hyprland version...${NC}"
HYPRLAND_VERSION=$(hyprctl version | head -n1 | grep -oP 'v\d+\.\d+\.\d+' || echo "unknown")
echo "Detected Hyprland version: $HYPRLAND_VERSION"

if [[ ! "$HYPRLAND_VERSION" =~ ^v0\.5[2-9] ]] && [[ "$HYPRLAND_VERSION" != "unknown" ]]; then
    echo -e "${YELLOW}Warning: These fixes are designed for Hyprland v0.52+${NC}"
    echo -e "${YELLOW}Your version: $HYPRLAND_VERSION${NC}"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo ""
echo -e "${YELLOW}Step 2: Cleaning previous build...${NC}"
make clean 2>/dev/null || true
rm -f build/*.so 2>/dev/null || true

echo ""
echo -e "${YELLOW}Step 3: Building Hyprspace...${NC}"
if make all; then
    echo -e "${GREEN}✓ Build successful!${NC}"
else
    echo -e "${RED}✗ Build failed!${NC}"
    echo "Check the error messages above."
    exit 1
fi

echo ""
echo -e "${YELLOW}Step 4: Checking if plugin is already loaded...${NC}"
if hyprpm list 2>/dev/null | grep -q "Hyprspace"; then
    echo "Hyprspace is currently loaded. Removing old version..."
    hyprpm remove Hyprspace || true
    sleep 1
fi

echo ""
echo -e "${YELLOW}Step 5: Installing plugin...${NC}"
if hyprpm add "$PWD"; then
    echo -e "${GREEN}✓ Plugin added successfully!${NC}"
else
    echo -e "${RED}✗ Failed to add plugin!${NC}"
    exit 1
fi

echo ""
echo -e "${YELLOW}Step 6: Enabling plugin...${NC}"
if hyprpm enable Hyprspace; then
    echo -e "${GREEN}✓ Plugin enabled successfully!${NC}"
else
    echo -e "${RED}✗ Failed to enable plugin!${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}=========================================="
echo "Build and Installation Complete!"
echo "==========================================${NC}"
echo ""
echo -e "${YELLOW}Testing Checklist:${NC}"
echo "  1. Open overview with your configured keybind"
echo "  2. Check if current workspace shows correctly"
echo "  3. Verify all workspaces display window content (not black)"
echo "  4. Test with VS Code - should show actual content, not wallpaper"
echo "  5. Test with Chrome/Electron apps"
echo "  6. Try switching between workspaces"
echo "  7. Drag and drop windows between workspaces"
echo "  8. Check for flickering when scrolling"
echo ""
echo -e "${YELLOW}To reload Hyprland configuration:${NC}"
echo "  hyprctl reload"
echo ""
echo -e "${YELLOW}To check plugin status:${NC}"
echo "  hyprpm list"
echo ""
echo -e "${YELLOW}If you encounter issues:${NC}"
echo "  1. Check Hyprland logs: journalctl -xe --user -u hyprland"
echo "  2. Try rebuilding: ./build-test.sh"
echo "  3. See HYPRLAND_0.52_FIXES.md for details"
echo ""
