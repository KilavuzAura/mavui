#!/usr/bin/env bash
# build.sh — MavUI derlemesi.
#
#   bash build.sh                 -> masaustu (Linux, sistem Qt'si)
#   bash build.sh android         -> Android APK (arm64-v8a) + imzalama
#   bash build.sh -t clean        -> ek argumanlar cmake --build'e gider
#   BUILD_DIR=build-test bash build.sh
#
# Android ortam degiskenleri (hepsinin makul varsayilani var):
#   QT_ANDROID=~/Qt/6.8.3/android_arm64_v8a   QT_HOST_PATH=~/Qt/6.8.3/gcc_64
#   ANDROID_SDK_ROOT=~/Android/Sdk            ANDROID_NDK_ROOT=<SDK/ndk icindeki en yenisi>
#   JAVA_HOME=~/Android/jdk-17                ANDROID_ABI=arm64-v8a
#   MAVUI_KEYSTORE=~/Android/mavui.keystore   MAVUI_KEY_ALIAS / MAVUI_KEYSTORE_PASS (istege bagli)
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"

HEDEF=masaustu
if [[ "${1:-}" == "android" || "${1:-}" == "--android" ]]; then
    HEDEF=android
    shift
fi

# ArduPilot parametre deposu alt modulu olmadan CMake yapilandirmasi patlar.
git submodule update --init --recursive

zorunlu_yol() {  # zorunlu_yol <aciklama> <yol>
    [[ -n "$2" && -e "$2" ]] || { echo "HATA: $1 bulunamadi: ${2:-<bos>}" >&2; exit 1; }
}

# Depo baska bir yoldan (or. symlink) yapilandirilmissa CMakeCache eski kaynak
# dizinini tasir; CPM o durumda kendini yuklemez ve yapilandirma
# "Unknown CMake command CPMAddPackage" ile duser. Sadece cache'i atiyoruz,
# nesne dosyalari kaliyor.
cache_yenile_gerekirse() {
    local cache="$1/CMakeCache.txt" eski
    [[ -f "$cache" ]] || return 0
    eski="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache")"
    # Karsilastirma metin bazli: symlink uzerinden yapilandirilmis dizin ayni
    # agaci gosterse bile CPM'in cachelenmis yolu tutmaz.
    [[ -n "$eski" && "$eski" != "$PWD" ]] || return 0
    echo "Not: $1 baska bir kaynak yolundan yapilandirilmis ($eski); CMakeCache yenileniyor."
    rm -f "$cache"
}

if [[ "$HEDEF" == "android" ]]; then
    BUILD_DIR="${BUILD_DIR:-build-android}"
    QT_ANDROID="${QT_ANDROID:-$HOME/Qt/6.8.3/android_arm64_v8a}"
    QT_HOST_PATH="${QT_HOST_PATH:-$HOME/Qt/6.8.3/gcc_64}"
    export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"
    NDK_SON="$(ls -d "$ANDROID_SDK_ROOT"/ndk/*/ 2>/dev/null | sort -V | tail -1 || true)"
    export ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-${NDK_SON%/}}"
    export JAVA_HOME="${JAVA_HOME:-$HOME/Android/jdk-17}"
    ABI="${ANDROID_ABI:-arm64-v8a}"

    zorunlu_yol "Qt Android kurulumu (QT_ANDROID)" "$QT_ANDROID/bin/qt-cmake"
    zorunlu_yol "Qt host kurulumu (QT_HOST_PATH)"  "$QT_HOST_PATH"
    zorunlu_yol "Android SDK (ANDROID_SDK_ROOT)"   "$ANDROID_SDK_ROOT"
    zorunlu_yol "Android NDK (ANDROID_NDK_ROOT)"   "$ANDROID_NDK_ROOT"
    zorunlu_yol "JDK 17 (JAVA_HOME)"               "$JAVA_HOME/bin/java"
    export PATH="$JAVA_HOME/bin:$PATH"

    # Android'de GStreamer kapali olmali: host kutuphaneleri aarch64 link'ine siziyor,
    # video QtMultimedia backend'iyle calisir.
    CMAKE_FLAGS=(-S . -B "$BUILD_DIR" -G Ninja
                 -DCMAKE_BUILD_TYPE=Release
                 -DQGC_BUILD_TESTING=OFF
                 -DQT_ANDROID_ABIS="$ABI"
                 -DQT_ANDROID_BUILD_ALL_ABIS=OFF
                 -DQT_HOST_PATH="$QT_HOST_PATH"
                 -DQT_ANDROID_SIGN_APK=OFF
                 -DQGC_ENABLE_GST_VIDEOSTREAMING=OFF
                 -DQGC_ENABLE_QT_VIDEOSTREAMING=ON)

    # Masaustundeki ile ayni gerekce: QGC ust akisi Qt 6.8.3'e kilitli.
    QT_VER="$("$QT_HOST_PATH/bin/qmake6" -query QT_VERSION 2>/dev/null || true)"
    [[ -n "$QT_VER" && "$QT_VER" != "6.8.3" ]] && CMAKE_FLAGS+=("-DQGC_QT_MAXIMUM_VERSION=$QT_VER")

    cache_yenile_gerekirse "$BUILD_DIR"
    "$QT_ANDROID/bin/qt-cmake" "${CMAKE_FLAGS[@]}"
    cmake --build "$BUILD_DIR" --target apk "$@"

    APK_DIR="$BUILD_DIR/android-build"
    APK="$APK_DIR/MavUI.apk"
    zorunlu_yol "uretilen APK" "$APK"

    # Imzalama: keystore varsa zipalign + apksigner, yoksa imzasiz APK ile birak.
    KEYSTORE="${MAVUI_KEYSTORE:-$HOME/Android/mavui.keystore}"
    BUILD_TOOLS="$(ls -d "$ANDROID_SDK_ROOT"/build-tools/*/ 2>/dev/null | sort -V | tail -1 || true)"
    BUILD_TOOLS="${BUILD_TOOLS%/}"
    if [[ -f "$KEYSTORE" && -x "$BUILD_TOOLS/zipalign" ]]; then
        ALIGNED="$APK_DIR/MavUI-aligned.apk"
        "$BUILD_TOOLS/zipalign" -p -f 4 "$APK" "$ALIGNED"
        SIGN_ARGS=(--ks "$KEYSTORE" --out "$APK_DIR/MavUI-signed.apk")
        [[ -n "${MAVUI_KEY_ALIAS:-}"    ]] && SIGN_ARGS+=(--ks-key-alias "$MAVUI_KEY_ALIAS")
        [[ -n "${MAVUI_KEYSTORE_PASS:-}" ]] && SIGN_ARGS+=(--ks-pass env:MAVUI_KEYSTORE_PASS)
        # Parola verilmediyse apksigner terminalden sorar; betik arka planda
        # calisiyorsa MAVUI_KEYSTORE_PASS sart.
        if "$BUILD_TOOLS/apksigner" sign "${SIGN_ARGS[@]}" "$ALIGNED"; then
            rm -f "$ALIGNED"
            echo
            echo "Hazir: $APK_DIR/MavUI-signed.apk"
            echo "Kurulum: adb install -r $APK_DIR/MavUI-signed.apk"
        else
            rm -f "$ALIGNED"
            echo
            echo "HATA: imzalama basarisiz (parola icin MAVUI_KEYSTORE_PASS)." >&2
            echo "Imzasiz APK yerinde: $APK" >&2
            exit 1
        fi
    else
        echo
        echo "Imzasiz APK: $APK"
        [[ -f "$KEYSTORE" ]] || echo "Keystore yok ($KEYSTORE). Olusturmak icin:"
        [[ -f "$KEYSTORE" ]] || echo "  keytool -genkeypair -v -keystore $KEYSTORE -alias mavui \\"
        [[ -f "$KEYSTORE" ]] || echo "    -keyalg RSA -keysize 2048 -validity 10000 -dname 'CN=MavUI, O=KilavuzAura'"
    fi
    exit 0
fi

BUILD_DIR="${BUILD_DIR:-build}"

CMAKE_FLAGS=(-S . -B "$BUILD_DIR" -G Ninja
             -DCMAKE_BUILD_TYPE=Release
             -DQGC_BUILD_TESTING=OFF)

# QGC ust akisi Qt 6.8.3'e kilitli. Arch gibi daha yeni Qt tasiyan sistemlerde
# ust siniri acmak ve qsb'yi elle gostermek gerekiyor (aksi halde yapilandirma
# "unsupported Qt version" ile duruyor).
QT_VER="$(qmake6 -query QT_VERSION 2>/dev/null || qtpaths6 --qt-version 2>/dev/null || true)"
if [[ -n "$QT_VER" && "$QT_VER" != "6.8.3" ]]; then
    CMAKE_FLAGS+=("-DQGC_QT_MAXIMUM_VERSION=$QT_VER")
    QSB="$(command -v qsb || true)"
    [[ -z "$QSB" && -x /usr/lib/qt6/bin/qsb ]] && QSB=/usr/lib/qt6/bin/qsb
    [[ -n "$QSB" ]] && CMAKE_FLAGS+=("-DQSB_PROGRAM=$QSB")
fi

# Yapilandirilmis dizinde cmake hizlica gecer; bayrak degistiyse yeniden kurar.
cache_yenile_gerekirse "$BUILD_DIR"
cmake "${CMAKE_FLAGS[@]}"
cmake --build "$BUILD_DIR" "$@"

echo
echo "Hazir: $BUILD_DIR/Release/MavUI"
