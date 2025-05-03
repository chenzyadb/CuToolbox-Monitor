ANDROID_NDK="d:/Apps/android-ndk"
SOURCE_DIR="c:/Users/chenz/Documents/GitHub/CuToolbox-Monitor"
BUILD_DIR="${SOURCE_DIR}/build"

mkdir -p "$BUILD_DIR"

cmake \
    -DCMAKE_BUILD_TYPE="release" \
    -DCMAKE_SYSTEM_NAME="Android" \
    -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK}/build/cmake/android.toolchain.cmake" \
    -DANDROID_NATIVE_API_LEVEL=28 \
    -DANDROID_ABI="arm64-v8a" \
    -DANDROID_STL="c++_static" \
    -H${SOURCE_DIR} \
    -B${BUILD_DIR} \
    -G "Ninja"
cmake --build "$BUILD_DIR" --config "release" --target "ct_monitor" -j16

cp -f "${BUILD_DIR}/ct_monitor" "${SOURCE_DIR}/"
rm -rf "$BUILD_DIR"