#!/usr/bin/sh

NDK_PATH="/home/android-ndk"
BASE_DIR=$(dirname $0)
BUILD_DIR="${BASE_DIR}/build"
SOURCE_DIR="${BASE_DIR}/source"
TOOLCHAIN_BIN="${NDK_PATH}/toolchains/llvm/prebuilt/linux-x86_64/bin"
TARGET_PREFIX="aarch64-linux-android28"

rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}

echo "- Build Monitor."
${TOOLCHAIN_BIN}/${TARGET_PREFIX}-clang++ \
"${BASE_DIR}/src/main.cpp" "${BASE_DIR}/src/utils/libcu.cpp" "${BASE_DIR}/src/utils/CuSimpleMatch.cpp" \
-static-libstdc++ -O3 -w \
-o "${BUILD_DIR}/ct_monitor"

echo "- Done."
exit 0;
