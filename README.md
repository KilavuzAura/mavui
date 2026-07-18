<p align="center">
  <img src="custom/res/Images/AuraLogo.png" alt="MavUI" width="220">
</p>

<h1 align="center">MavUI</h1>

<p align="center"><b>AURA (KilavuzAura) su altı aracı takımının yer kontrol istasyonu</b><br>
QGroundControl <code>Stable_V5.0</code> tabanlı özel derleme — ArduSub / Orange Cube+ için ayarlandı.</p>

---

## Ne farklı?

QGC'nin resmi **custom build** mekanizması (`custom/` klasörü) ile markalama ve davranış özelleştirmeleri; ayrıca ağaçta bir dizi düzeltme:

- **AURA markası** — uygulama adı MavUI, Tokyo Night renk paleti, AURA logosu/ikonları
- **ArduSub odaklı** — çevrimdışı plan düzenleme varsayılanı ArduPilot/Sub, tek araç arayüzü
- **Çalışan harita** — varsayılan sağlayıcı Esri World Satellite (stok QGC'nin varsayılanı Bing; servis kapandığı için boş harita gösteriyor)
- **Android dosya erişimi** — sistem dosya seçicisi (SAF) ile cihazın tamamından dosya seçme; ilk açılışta "Tüm dosyalara erişim" izni istenir; uygulama verileri görünür `Documents/MavUI` klasöründe tutulur
- **Kendi Android kimliği** — `tr.com.aurateam.mavui`, stok QGC ile yan yana kurulabilir
- **Düzeltmeler** — QtMultimedia video backend'inde açılışta çökme, log handler'ının warning/error yutması, Qt 6.11 / GCC 16 / GStreamer 1.28 derleme uyumu

## Derleme

### Masaüstü (Linux, sistem Qt'siyle)

Gereksinimler: Qt ≥ 6.8 (Arch'ta 6.11 ile test edildi), CMake ≥ 3.22, Ninja, GStreamer 1.x.

```bash
git submodule update --init   # ArduPilot parametre deposu
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DQGC_BUILD_TESTING=OFF \
      -DQGC_QT_MAXIMUM_VERSION=6.11.1 \
      -DQSB_PROGRAM=/usr/lib/qt6/bin/qsb
cmake --build build
./build/Release/MavUI
```

> Qt 6.8.3 kullanıyorsanız `QGC_QT_MAXIMUM_VERSION` ve `QSB_PROGRAM` bayraklarına gerek yok.

### Android (arm64-v8a)

Gereksinimler: Qt 6.8.3 (host + `android_arm64_v8a`), Android SDK (platform 34/35, build-tools 34), NDK r26b, JDK 17.

```bash
export ANDROID_SDK_ROOT=~/Android/Sdk
export ANDROID_NDK_ROOT=~/Android/Sdk/ndk/26.1.10909125
export JAVA_HOME=~/Android/jdk-17

~/Qt/6.8.3/android_arm64_v8a/bin/qt-cmake -S . -B build-android -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DQT_ANDROID_ABIS=arm64-v8a -DQT_ANDROID_BUILD_ALL_ABIS=OFF \
    -DQT_HOST_PATH=~/Qt/6.8.3/gcc_64 -DQT_ANDROID_SIGN_APK=OFF \
    -DQGC_ENABLE_GST_VIDEOSTREAMING=OFF -DQGC_ENABLE_QT_VIDEOSTREAMING=ON
cmake --build build-android --target all
# APK: build-android/android-build/MavUI.apk  (zipalign + apksigner ile imzalayın)
```

> Android'de GStreamer kapalı olmalı (host kütüphaneleri aarch64 link'ine sızıyor); video QtMultimedia backend'iyle çalışır.

## Araca bağlanma (BlueOS)

Araç ağında (`192.168.2.0/24`) BlueOS `192.168.2.2`'dedir. Masaüstünde `192.168.2.1` alan topside otomatik bağlanır. Tablet/telefon gibi DHCP ile IP alan cihazlarda: **Application Settings → Comm Links** → yeni **UDP** link → hedef `192.168.2.2:14550` (veya BlueOS'ta tanımlı ek `udpin` portu) → Connect.

## Depo düzeni

| Yer | İçerik |
|---|---|
| `custom/` | MavUI overlay'i: markalama, varsayılanlar, Android kimliği/ikonları |
| `main` branch | Bu proje (QGC Stable_V5.0 + MavUI commit'leri) |
| `custom` branch | Takımın önceki el yazması Qt Widgets GCS'i (arşiv) |

## Lisans ve üst kaynak

Bu proje [QGroundControl](https://github.com/mavlink/qgroundcontrol)'un bir çatallamasıdır ve üst projenin çifte lisansına tabidir: **Apache 2.0** ve **GPLv3** (bkz. `LICENSE-APACHE`, `LICENSE-GPL`). QGroundControl, Dronecode Projesi'nin tescilli markasıdır; bu depo resmi bir QGC dağıtımı değildir.
