#!/usr/bin/env bash
# android_kayit_testi.sh — MavUI Android APK'si GERCEKTEN video dosyasi yaziyor mu?
#
# NEDEN VAR: Android APK'si uzun sure QtMultimedia ile derlendi; kayit butonu
# vardi ama dosya YAZILMIYORDU (bkz. CLAUDE.md "QGC kayit butonu / CAM1_TYPE").
# GStreamer'li derlemeden sonra bunun kanitla dogrulanmasi gerekiyor: butona
# basmak yeterli degil, dosya cikmali ve ffprobe onu decode edebilmeli.
#
# Telefon ile bu makine AYNI Wi-Fi'da olmali (UDP; adb forward/reverse UDP
# tasimaz, o yuzden USB tek basina yetmez).
#
# Kullanim:
#   ./android_kayit_testi.sh kur                 # APK'yi imzala + telefona kur
#   ./android_kayit_testi.sh yayin <telefon_ip>  # sahte arac + H264 akisi yayinla
#   ./android_kayit_testi.sh cek                 # kaydi telefondan cek + dogrula
set -euo pipefail

KOK="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ADB="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
BT="${BUILD_TOOLS:-$HOME/Android/Sdk/build-tools/35.0.0}"
APK_HAM="${APK:-$KOK/build-android-gst/android-build/MavUI.apk}"
APK_IMZALI="${APK_HAM%.apk}-gst-signed.apk"
KEYSTORE="${MAVUI_KEYSTORE:-$HOME/Android/mavui.keystore}"
PASS_DOSYA="${MAVUI_KEYSTORE_PASS_FILE:-$HOME/Android/mavui-keystore-pass.txt}"
# AndroidManifest.xml hala org.mavlink.qgroundcontrol yaziyor ama gercek paket
# derlemede degistiriliyor; derlenmis APK'ya sorulduğunda cikan bu (aapt2 dump
# packagename). Stok QGC ile yan yana kurulabilmesinin sebebi de bu.
PAKET="${PAKET:-tr.com.aurateam.mavui}"
VIDEO_PORT="${VIDEO_PORT:-5600}"
MAVLINK_PORT="${MAVLINK_PORT:-14550}"

ok()   { printf '  \033[32m✓\033[0m %s\n' "$*"; }
hata() { printf '  \033[31m✗\033[0m %s\n' "$*"; }
baslik(){ printf '\n\033[1m%s\033[0m\n' "$*"; }

# ---------------------------------------------------------------- kur
if [[ "${1:-}" == "kur" ]]; then
    baslik "APK imzala"
    [[ -f "$APK_HAM" ]] || { hata "APK yok: $APK_HAM (once ./build.sh android)"; exit 1; }
    PASS="$(cat "$PASS_DOSYA")"
    ALIAS="$(keytool -list -keystore "$KEYSTORE" -storepass "$PASS" 2>/dev/null \
             | grep -i PrivateKeyEntry | head -1 | cut -d, -f1)"
    "$BT/zipalign" -p -f 4 "$APK_HAM" "$APK_IMZALI"
    MAVUI_KEYSTORE_PASS="$PASS" "$BT/apksigner" sign --ks "$KEYSTORE" \
        --ks-key-alias "$ALIAS" --ks-pass env:MAVUI_KEYSTORE_PASS "$APK_IMZALI"
    "$BT/apksigner" verify "$APK_IMZALI" && ok "imzalandi: $APK_IMZALI"

    baslik "Telefona kur"
    "$ADB" devices | sed 's/^/  /'
    if ! "$ADB" get-state >/dev/null 2>&1; then
        hata "Telefon gorunmuyor. USB hata ayiklama acik mi? Kabloyu tak, telefondaki izni onayla."
        exit 1
    fi
    # -r: mevcut kurulumu koru; imza degistiyse -r yetmez, once kaldirmak gerekir.
    "$ADB" install -r "$APK_IMZALI" || {
        hata "install -r basarisiz (muhtemelen farkli imza). Kaldirip tekrar deniyorum."
        "$ADB" uninstall "$PAKET" || true
        "$ADB" install "$APK_IMZALI"
    }
    ok "kuruldu"
    exit 0
fi

# ---------------------------------------------------------------- yayin
if [[ "${1:-}" == "yayin" ]]; then
    IP="${2:-}"
    [[ -n "$IP" ]] || { hata "telefon IP'si lazim: ./android_kayit_testi.sh yayin 192.168.49.x"; exit 1; }

    baslik "Once KENDI akisimizi dogrula (telefonu suclamadan once)"
    # Ayni pipeline'i 127.0.0.1'e de yollayip burada decode ediyoruz. Bu gecmezse
    # sorun gonderendedir, telefonda degil -- testin en sik bosa gittigi yer.
    gst-launch-1.0 -q videotestsrc num-buffers=60 pattern=ball \
        ! video/x-raw,width=640,height=480,framerate=30/1 \
        ! x264enc tune=zerolatency key-int-max=15 ! rtph264pay config-interval=1 pt=96 \
        ! udpsink host=127.0.0.1 port=5699 >/dev/null 2>&1 &
    GONDEREN=$!
    if timeout 8 gst-launch-1.0 -q udpsrc port=5699 \
        caps="application/x-rtp,media=video,clock-rate=90000,encoding-name=H264,payload=96" \
        num-buffers=20 ! rtph264depay ! h264parse ! fakesink >/dev/null 2>&1; then
        ok "gonderici pipeline saglam"
    else
        hata "kendi akisimiz bile okunamadi -- gst kurulumuna bak"; kill $GONDEREN 2>/dev/null || true; exit 1
    fi
    wait $GONDEREN 2>/dev/null || true

    baslik "Yayin basliyor -> $IP"
    echo "  video   : udp://$IP:$VIDEO_PORT  (H264/RTP)"
    echo "  mavlink : udp://$IP:$MAVLINK_PORT (sahte arac heartbeat)"
    echo
    echo "  Telefonda MavUI: Application Settings > Video > Source = UDP h.264,"
    echo "  Port = $VIDEO_PORT. Arac baglanınca kayit butonu gorunur."
    echo "  Kaydi baslat, ~15 sn bekle, durdur. Sonra: ./android_kayit_testi.sh cek"
    echo
    echo "  Durdurmak icin Ctrl-C."

    # Sahte arac: MavUI'nin arac gormesi sart -- kayit butonu bir kamera nesnesine
    # bagli (PhotoVideoControl.qml: _camera && ...). Arac yoksa buton hic cikmaz.
    python3 - "$IP" "$MAVLINK_PORT" <<'PY' &
import sys, time
from pymavlink import mavutil
ip, port = sys.argv[1], int(sys.argv[2])
m = mavutil.mavlink_connection(f"udpout:{ip}:{port}", source_system=1, source_component=1)
while True:
    m.mav.heartbeat_send(mavutil.mavlink.MAV_TYPE_SUBMARINE,
                         mavutil.mavlink.MAV_AUTOPILOT_ARDUPILOTMEGA,
                         mavutil.mavlink.MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, 0,
                         mavutil.mavlink.MAV_STATE_STANDBY)
    m.mav.sys_status_send(0, 0, 0, 500, 12000, -1, -1, 0, 0, 0, 0, 0, 0)
    time.sleep(1)
PY
    MAVPID=$!
    trap 'kill $MAVPID 2>/dev/null || true; exit 0' INT TERM

    # config-interval=1: SPS/PPS saniyede bir tekrar gonderilir; alici yayina
    # ortadan katilsa bile decode edebilsin (kayit ortadan baslatiliyor).
    gst-launch-1.0 -q videotestsrc is-live=true pattern=ball \
        ! video/x-raw,width=1280,height=720,framerate=30/1 \
        ! timeoverlay ! x264enc tune=zerolatency bitrate=2000 key-int-max=30 \
        ! rtph264pay config-interval=1 pt=96 ! udpsink host="$IP" port="$VIDEO_PORT"
    exit 0
fi

# ---------------------------------------------------------------- cek
if [[ "${1:-}" == "cek" ]]; then
    baslik "Telefondaki kayitlari bul"
    HEDEF="/tmp/mavui_android_kayit"; mkdir -p "$HEDEF"
    # MavUI videoyu <savePath>/Video altina yazar (AppSettings::videoSavePath).
    # Bu forkta savePath gorunur bir yer: Documents/MavUI (README). Once orayi,
    # bulamazsak sdcard genelini tara -- kullanici savePath'i degistirmis olabilir.
    BULUNAN="$("$ADB" shell "ls -1 /sdcard/Documents/MavUI/Video/* 2>/dev/null" | tr -d '\r' || true)"
    if [[ -z "$BULUNAN" ]]; then
        echo "  Documents/MavUI/Video bos, sdcard taraniyor..."
        BULUNAN="$("$ADB" shell "find /sdcard -iname '*.mkv' -o -iname '*.mp4' 2>/dev/null | head -20" | tr -d '\r' || true)"
    fi
    if [[ -z "$BULUNAN" ]]; then
        hata "Kayit dosyasi bulunamadi."
        echo "  Kontrol: MavUI'de kayit butonuna basildi mi? Depolama izni verildi mi?"
        echo "  Elle bak: $ADB shell find /sdcard -iname '*.mkv'"
        exit 1
    fi
    echo "$BULUNAN" | sed 's/^/  /'
    while read -r f; do
        [[ -n "$f" ]] || continue
        "$ADB" pull "$f" "$HEDEF/" >/dev/null 2>&1 || continue
    done <<< "$BULUNAN"

    baslik "Dogrulama (dosya var demek yetmez, decode edilmeli)"
    SAYAC=0
    for f in "$HEDEF"/*.mkv "$HEDEF"/*.mp4; do
        [[ -f "$f" ]] || continue
        BOYUT=$(stat -c%s "$f")
        SURE=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$f" 2>/dev/null || true)
        KARE=$(ffprobe -v error -select_streams v:0 -count_packets -show_entries stream=nb_read_packets -of csv=p=0 "$f" 2>/dev/null || true)
        if [[ -n "$SURE" && "$SURE" != "N/A" ]]; then
            ok "$(basename "$f"): ${BOYUT}B  sure=${SURE}s  paket=${KARE}"
            SAYAC=$((SAYAC+1))
        else
            hata "$(basename "$f"): ${BOYUT}B  ama ffprobe SURE OKUYAMADI (finalize edilmemis)"
        fi
    done
    echo
    (( SAYAC > 0 )) && { ok "APK gercekten video yaziyor."; exit 0; }
    hata "Decode edilebilir kayit yok."; exit 1
fi

awk 'NR>1 && /^#/ { sub(/^# ?/, ""); print; next } NR>1 { exit }' "$0"
exit 1
