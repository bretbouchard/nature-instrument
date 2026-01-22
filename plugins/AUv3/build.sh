#!/bin/bash

# Build script for Nature Marco AUv3 iOS Plugin

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
XCODE_PROJECT="${PROJECT_DIR}/NaturePlugin.xcodeproj"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Nature Marco AUv3 iOS Build Script${NC}"
echo "======================================"

# Parse arguments
SCHEME="NaturePlugin"
CONFIGURATION="Release"
SDK="iphoneos"
DESTINATION="generic/platform=iOS"

while [[ $# -gt 0 ]]; do
  case $1 in
    --debug)
      CONFIGURATION="Debug"
      shift
      ;;
    --simulator)
      SDK="iphonesimulator"
      DESTINATION="platform=iOS Simulator,name=iPhone 15"
      shift
      ;;
    --clean)
      CLEAN=true
      shift
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

echo "Configuration: ${CONFIGURATION}"
echo "SDK: ${SDK}"
echo ""

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "${BUILD_DIR}"
fi

# Build
echo -e "${GREEN}Building NaturePlugin...${NC}"

xcodebuild \
  -project "${XCODE_PROJECT}" \
  -scheme "${SCHEME}" \
  -configuration "${CONFIGURATION}" \
  -sdk "${SDK}" \
  -destination "${DESTINATION}" \
  -derivedDataPath "${BUILD_DIR}" \
  build

# Check build result
if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}Build successful!${NC}"
    echo ""
    echo "Output: ${BUILD_DIR}/Build/Products/${CONFIGURATION}-${SDK}/"
    echo ""
    echo "To install on device:"
    echo "1. Connect iOS device"
    echo "2. Open Xcode organizer"
    echo "3. Archive and distribute"
else
    echo ""
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
