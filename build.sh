#!/usr/bin/env bash
# build.sh — MavUI masaustu derlemesi (Linux, sistem Qt'si).
#
#   bash build.sh                 -> yapilandir + derle
#   bash build.sh -t clean        -> ek argumanlar cmake --build'e gider
#   BUILD_DIR=build-test bash build.sh
#
# Android APK derlemesi ayridir, README.md'ye bak.
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"

BUILD_DIR="${BUILD_DIR:-build}"

# ArduPilot parametre deposu alt modulu olmadan CMake yapilandirmasi patlar.
git submodule update --init --recursive

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
cmake "${CMAKE_FLAGS[@]}"
cmake --build "$BUILD_DIR" "$@"

echo
echo "Hazir: $BUILD_DIR/Release/MavUI"
