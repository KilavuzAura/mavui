#!/usr/bin/env bash
# build.sh - MavUI build.
#
#   bash build.sh                 -> desktop (Linux, system Qt)
#   bash build.sh android         -> Android APK (arm64-v8a) + signing
#   bash build.sh -t clean        -> extra arguments are passed to cmake --build
#   BUILD_DIR=build-test bash build.sh
#
# Android environment variables (all have sensible defaults):
#   QT_ANDROID=~/Qt/6.8.3/android_arm64_v8a   QT_HOST_PATH=~/Qt/6.8.3/gcc_64
#   ANDROID_SDK_ROOT=~/Android/Sdk            ANDROID_NDK_ROOT=<newest under SDK/ndk>
#   JAVA_HOME=~/Android/jdk-17                ANDROID_ABI=arm64-v8a
#   MAVUI_KEYSTORE=~/Android/mavui.keystore   MAVUI_KEY_ALIAS / MAVUI_KEYSTORE_PASS (optional)
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"

TARGET=desktop
if [[ "${1:-}" == "android" || "${1:-}" == "--android" ]]; then
    TARGET=android
    shift
fi

# Without the ArduPilot parameter-repository submodule the CMake configure step fails.
git submodule update --init --recursive

require_path() {  # require_path <description> <path>
    [[ -n "$2" && -e "$2" ]] || { echo "ERROR: $1 not found: ${2:-<empty>}" >&2; exit 1; }
}

# If the tree was configured from a different path (e.g. a symlink), CMakeCache
# carries the old source directory; CPM then fails to bootstrap itself and the
# configure step dies with "Unknown CMake command CPMAddPackage". We drop only
# the cache; the object files stay.
refresh_cache_if_needed() {
    local cache="$1/CMakeCache.txt" prev
    [[ -f "$cache" ]] || return 0
    prev="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache")"
    # The comparison is textual: even if a directory configured through a symlink
    # points at the same tree, CPM's cached path does not survive it.
    [[ -n "$prev" && "$prev" != "$PWD" ]] || return 0
    echo "Note: $1 was configured from a different source path ($prev); refreshing CMakeCache."
    rm -f "$cache"
}

if [[ "$TARGET" == "android" ]]; then
    BUILD_DIR="${BUILD_DIR:-build-android}"
    QT_ANDROID="${QT_ANDROID:-$HOME/Qt/6.8.3/android_arm64_v8a}"
    QT_HOST_PATH="${QT_HOST_PATH:-$HOME/Qt/6.8.3/gcc_64}"
    export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"
    NDK_LATEST="$(ls -d "$ANDROID_SDK_ROOT"/ndk/*/ 2>/dev/null | sort -V | tail -1 || true)"
    export ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-${NDK_LATEST%/}}"
    export JAVA_HOME="${JAVA_HOME:-$HOME/Android/jdk-17}"
    ABI="${ANDROID_ABI:-arm64-v8a}"

    require_path "Qt Android installation (QT_ANDROID)" "$QT_ANDROID/bin/qt-cmake"
    require_path "Qt host installation (QT_HOST_PATH)" "$QT_HOST_PATH"
    require_path "Android SDK (ANDROID_SDK_ROOT)"   "$ANDROID_SDK_ROOT"
    require_path "Android NDK (ANDROID_NDK_ROOT)"   "$ANDROID_NDK_ROOT"
    require_path "JDK 17 (JAVA_HOME)"               "$JAVA_HOME/bin/java"
    export PATH="$JAVA_HOME/bin:$PATH"

    # GStreamer is required on Android too: recording is FAKE on the QtMultimedia
    # backend. QtMultimediaReceiver builds a QMediaCaptureSession but never
    # connects the _mediaPlayer that carries the stream to it, so the capture
    # session has no input; startRecording() calls record() anyway and emits
    # recordingChanged(true). Result: the button turns red, the counter runs and
    # not one byte reaches the disk. It is an upstream QGC defect, but as long as
    # we build the APK with QtMultimedia we ship a button that lies to the user.
    #
    # GStreamer had previously been disabled over "host libraries leaking into the
    # aarch64 link". The real cause was found (7 Aug 2026): the .pc files of
    # GStreamer's libav plugin ask for -latomic, CMake resolves that with
    # find_library, and the NDK sysroot has NO libatomic (it lives in clang's own
    # runtime directory) -> the search falls through to the host and finds
    # /usr/lib/libatomic.so -> "ld: error: /usr/lib/libatomic.so is incompatible
    # with aarch64linux". libEGL was coming from the host the same way.
    # The fix rests on the SEARCH PATH, not on a variable name: we make
    # find_library look in the NDK's own directories first, which closes the whole
    # class (libatomic/libEGL today, something else tomorrow).
    unset PKG_CONFIG_PATH PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR
    NDK_TC="$(ls -d "$ANDROID_NDK_ROOT"/toolchains/llvm/prebuilt/*/ 2>/dev/null | head -1)"
    NDK_TC="${NDK_TC%/}"
    case "$ABI" in
        arm64-v8a)   NDK_TRIPLE=aarch64-linux-android; NDK_RT_ARCH=aarch64 ;;
        armeabi-v7a) NDK_TRIPLE=arm-linux-androideabi;  NDK_RT_ARCH=arm ;;
        x86_64)      NDK_TRIPLE=x86_64-linux-android;   NDK_RT_ARCH=x86_64 ;;
        x86)         NDK_TRIPLE=i686-linux-android;     NDK_RT_ARCH=i386 ;;
        *)           NDK_TRIPLE=""; NDK_RT_ARCH="" ;;
    esac
    NDK_LIB_PATHS=()
    if [[ -n "$NDK_TRIPLE" && -n "$NDK_TC" ]]; then
        # clang runtime (libatomic lives here, not in the sysroot)
        RT="$(ls -d "$NDK_TC"/lib/clang/*/lib/linux/"$NDK_RT_ARCH" 2>/dev/null | sort -V | tail -1)"
        [[ -d "$RT" ]] && NDK_LIB_PATHS+=("$RT")
        # platform libraries (libEGL etc.); highest API level
        API="$(ls "$NDK_TC/sysroot/usr/lib/$NDK_TRIPLE" 2>/dev/null | grep -E '^[0-9]+$' | sort -n | tail -1)"
        [[ -n "$API" ]] && NDK_LIB_PATHS+=("$NDK_TC/sysroot/usr/lib/$NDK_TRIPLE/$API")
    fi
    if (( ${#NDK_LIB_PATHS[@]} )); then
        NDK_LIB_PATH="$(IFS=';'; echo "${NDK_LIB_PATHS[*]}")"
        echo "NDK library search path: $NDK_LIB_PATH"
    else
        NDK_LIB_PATH=""
        echo "WARNING: NDK library paths not found; host libraries may leak in."
    fi
    CMAKE_FLAGS=(-S . -B "$BUILD_DIR" -G Ninja
                 -DCMAKE_BUILD_TYPE=Release
                 -DQGC_BUILD_TESTING=OFF
                 -DQT_ANDROID_ABIS="$ABI"
                 -DQT_ANDROID_BUILD_ALL_ABIS=OFF
                 -DQT_HOST_PATH="$QT_HOST_PATH"
                 -DQT_ANDROID_SIGN_APK=OFF
                 -DQGC_ENABLE_GST_VIDEOSTREAMING=ON
                 -DQGC_ENABLE_QT_VIDEOSTREAMING=OFF)
    [[ -n "$NDK_LIB_PATH" ]] && CMAKE_FLAGS+=("-DCMAKE_LIBRARY_PATH=$NDK_LIB_PATH")

    # GStreamer's Android package is 419 MB and cmake's downloader breaks off
    # halfway on a proxied network (CPM/FetchContent does not retry, the whole
    # configure fails). Use an already-extracted local copy if there is one;
    # otherwise cmake tries the download itself. To prepare it:
    #   mkdir -p ~/Android/gst-cache && cd ~/Android/gst-cache
    #   curl -LC - -O https://gstreamer.freedesktop.org/data/pkg/android/1.22.12/gstreamer-1.0-android-universal-1.22.12.tar.xz
    #   mkdir -p gstreamer-1.0-android-universal-1.22.12
    #   tar xf gstreamer-1.0-android-universal-1.22.12.tar.xz -C gstreamer-1.0-android-universal-1.22.12
    GST_ANDROID_DIR="${GST_ANDROID_DIR:-$HOME/Android/gst-cache/gstreamer-1.0-android-universal-1.22.12}"
    if [[ -d "$GST_ANDROID_DIR/arm64" ]]; then
        echo "GStreamer Android package from local copy: $GST_ANDROID_DIR"
        CMAKE_FLAGS+=("-DCPM_gstreamer_SOURCE=$GST_ANDROID_DIR")
    fi

    # Same rationale as on the desktop: QGC upstream is pinned to Qt 6.8.3.
    QT_VER="$("$QT_HOST_PATH/bin/qmake6" -query QT_VERSION 2>/dev/null || true)"
    [[ -n "$QT_VER" && "$QT_VER" != "6.8.3" ]] && CMAKE_FLAGS+=("-DQGC_QT_MAXIMUM_VERSION=$QT_VER")

    refresh_cache_if_needed "$BUILD_DIR"
    "$QT_ANDROID/bin/qt-cmake" "${CMAKE_FLAGS[@]}"
    cmake --build "$BUILD_DIR" --target apk "$@"

    APK_DIR="$BUILD_DIR/android-build"
    APK="$APK_DIR/MavUI.apk"
    require_path "generated APK" "$APK"

    # Signing: with a keystore, zipalign + apksigner; otherwise leave the APK unsigned.
    KEYSTORE="${MAVUI_KEYSTORE:-$HOME/Android/mavui.keystore}"
    BUILD_TOOLS="$(ls -d "$ANDROID_SDK_ROOT"/build-tools/*/ 2>/dev/null | sort -V | tail -1 || true)"
    BUILD_TOOLS="${BUILD_TOOLS%/}"
    if [[ -f "$KEYSTORE" && -x "$BUILD_TOOLS/zipalign" ]]; then
        ALIGNED="$APK_DIR/MavUI-aligned.apk"
        "$BUILD_TOOLS/zipalign" -p -f 4 "$APK" "$ALIGNED"
        SIGN_ARGS=(--ks "$KEYSTORE" --out "$APK_DIR/MavUI-signed.apk")
        [[ -n "${MAVUI_KEY_ALIAS:-}"    ]] && SIGN_ARGS+=(--ks-key-alias "$MAVUI_KEY_ALIAS")
        [[ -n "${MAVUI_KEYSTORE_PASS:-}" ]] && SIGN_ARGS+=(--ks-pass env:MAVUI_KEYSTORE_PASS)
        # Without a password apksigner prompts on the terminal; when the script
        # runs in the background MAVUI_KEYSTORE_PASS is mandatory.
        if "$BUILD_TOOLS/apksigner" sign "${SIGN_ARGS[@]}" "$ALIGNED"; then
            rm -f "$ALIGNED"
            echo
            echo "Ready: $APK_DIR/MavUI-signed.apk"
            echo "Install: adb install -r $APK_DIR/MavUI-signed.apk"
        else
            rm -f "$ALIGNED"
            echo
            echo "ERROR: signing failed (set MAVUI_KEYSTORE_PASS for the password)." >&2
            echo "The unsigned APK is still there: $APK" >&2
            exit 1
        fi
    else
        echo
        echo "Unsigned APK: $APK"
        [[ -f "$KEYSTORE" ]] || echo "No keystore ($KEYSTORE). To create one:"
        [[ -f "$KEYSTORE" ]] || echo "  keytool -genkeypair -v -keystore $KEYSTORE -alias mavui \\"
        [[ -f "$KEYSTORE" ]] || echo "    -keyalg RSA -keysize 2048 -validity 10000 -dname 'CN=MavUI, O=KilavuzAura'"
    fi
    exit 0
fi

BUILD_DIR="${BUILD_DIR:-build}"

CMAKE_FLAGS=(-S . -B "$BUILD_DIR" -G Ninja
             -DCMAKE_BUILD_TYPE=Release
             -DQGC_BUILD_TESTING=OFF)

# QGC upstream is pinned to Qt 6.8.3. On systems carrying a newer Qt (Arch, for
# one) the upper bound has to be lifted and qsb pointed at by hand, otherwise the
# configure step stops with "unsupported Qt version".
QT_VER="$(qmake6 -query QT_VERSION 2>/dev/null || qtpaths6 --qt-version 2>/dev/null || true)"
if [[ -n "$QT_VER" && "$QT_VER" != "6.8.3" ]]; then
    CMAKE_FLAGS+=("-DQGC_QT_MAXIMUM_VERSION=$QT_VER")
    QSB="$(command -v qsb || true)"
    [[ -z "$QSB" && -x /usr/lib/qt6/bin/qsb ]] && QSB=/usr/lib/qt6/bin/qsb
    [[ -n "$QSB" ]] && CMAKE_FLAGS+=("-DQSB_PROGRAM=$QSB")
fi

# In an already-configured directory cmake passes quickly; it reconfigures if a flag changed.
refresh_cache_if_needed "$BUILD_DIR"
cmake "${CMAKE_FLAGS[@]}"
cmake --build "$BUILD_DIR" "$@"

echo
echo "Ready: $BUILD_DIR/Release/MavUI"
