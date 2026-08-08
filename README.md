<p align="center">
  <img src="custom/res/Images/AuraLogo.png" alt="MavUI" width="220">
</p>

<h1 align="center">MavUI</h1>

<p align="center"><b>AURA (KilavuzAura) su altı aracı takımının yer kontrol istasyonu</b><br>
QGroundControl <code>Stable_V5.0</code> tabanlı özel derleme — ArduSub için ayarlandı.</p>

---

## Ne farklı?

QGC'nin resmi **custom build** mekanizması (`custom/` klasörü) ile markalama ve davranış özelleştirmeleri; ayrıca ağaçta bir dizi düzeltme:

- **AURA markası** — uygulama adı MavUI, Tokyo Night renk paleti, AURA logosu/ikonları
- **ArduSub odaklı** — çevrimdışı plan düzenleme varsayılanı ArduPilot/Sub, tek araç arayüzü
- **Android dosya erişimi** — sistem dosya seçicisi (SAF) ile cihazın tamamından dosya seçme; ilk açılışta "Tüm dosyalara erişim" izni istenir; uygulama verileri görünür `Documents/MavUI` klasöründe tutulur
- **Kendi Android kimliği** — `tr.com.aurateam.mavui`, stok QGC ile yan yana kurulabilir

## Derleme

Her iki hedef de `build.sh` ile derlenir; betik Qt sürümünü, araç yollarını ve
alt modülleri kendisi ayarlar.

```bash
bash build.sh            # masaüstü -> build/Release/MavUI
bash build.sh android    # APK      -> build-android/android-build/MavUI-signed.apk
```

### Masaüstü (Linux, sistem Qt'siyle)

Gereksinimler: Qt ≥ 6.8 (Arch'ta 6.11 ile test edildi), CMake ≥ 3.22, Ninja, GStreamer 1.x.

Betiğin yaptığı işin elle karşılığı:

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

`bash build.sh android` yapılandırır, derler, `zipalign` + `apksigner` ile imzalar.
Varsayılan yollar ortam değişkenleriyle değiştirilebilir:

| Değişken | Varsayılan |
|---|---|
| `QT_ANDROID` / `QT_HOST_PATH` | `~/Qt/6.8.3/android_arm64_v8a` / `~/Qt/6.8.3/gcc_64` |
| `ANDROID_SDK_ROOT` / `ANDROID_NDK_ROOT` | `~/Android/Sdk` / SDK içindeki en yeni NDK |
| `JAVA_HOME` | `~/Android/jdk-17` |
| `ANDROID_ABI` | `arm64-v8a` |
| `MAVUI_KEYSTORE` | `~/Android/mavui.keystore` (yoksa APK imzasız bırakılır) |
| `MAVUI_KEY_ALIAS` / `MAVUI_KEYSTORE_PASS` | boş — apksigner parolayı sorar |

#### Video kaydı Android'de gerçekten çalışıyor mu?

APK uzun süre QtMultimedia ile derlendi; **kayıt butonu vardı ama dosya
yazılmıyordu.** GStreamer'li derlemede (`QGC_ENABLE_GST_VIDEOSTREAMING=ON`)
bunun kanıtla doğrulanması gerekiyor — butona basılması yetmez, dosya çıkmalı
ve `ffprobe` onu decode edebilmeli:

```bash
./android_kayit_testi.sh kur                 # imzala + telefona kur
./android_kayit_testi.sh yayin 192.168.x.y   # sahte araç + sentetik H264/RTP
./android_kayit_testi.sh cek                 # kaydı çek + ffprobe ile doğrula
```

Telefon ile geliştirme makinesi **aynı Wi-Fi'da** olmalı: akış UDP, `adb
forward`/`reverse` ise yalnız TCP taşır. Sahte araç şart, çünkü kayıt butonu
bir kamera nesnesine bağlı (`PhotoVideoControl.qml`) — araç yoksa buton hiç
görünmez.

Betiğin yaptığı işin elle karşılığı:

```bash
export ANDROID_SDK_ROOT=~/Android/Sdk
export ANDROID_NDK_ROOT=~/Android/Sdk/ndk/26.1.10909125
export JAVA_HOME=~/Android/jdk-17

~/Qt/6.8.3/android_arm64_v8a/bin/qt-cmake -S . -B build-android -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DQT_ANDROID_ABIS=arm64-v8a -DQT_ANDROID_BUILD_ALL_ABIS=OFF \
    -DQT_HOST_PATH=~/Qt/6.8.3/gcc_64 -DQT_ANDROID_SIGN_APK=OFF \
    -DQGC_ENABLE_GST_VIDEOSTREAMING=OFF -DQGC_ENABLE_QT_VIDEOSTREAMING=ON
cmake --build build-android --target apk
# APK: build-android/android-build/MavUI.apk  (zipalign + apksigner ile imzalayın)
```

> Android'de GStreamer kapalı olmalı (host kütüphaneleri aarch64 link'ine sızıyor); video QtMultimedia backend'iyle çalışır.

### Windows ve sürüm çıkarma (GitHub Actions)

Windows sürümü Linux'tan çapraz derlenmez (Qt hazır çapraz kit vermiyor, GStreamer + QtLocation yığınını elle derlemek gerekir); MSVC 2022 + Qt 6.8.3 `win64_msvc2022_64` ile CI'da derlenir.

| Workflow | Ne yapar | Çıktı |
|---|---|---|
| `windows.yml` | Actions → Windows → Run workflow | `MavUI-installer.exe` (artifact) |
| `linux.yml` | Actions → Linux → Run workflow | `MavUI-x86_64.AppImage` (artifact) |
| `android-linux.yml` | Actions → Android-Linux → Run workflow | `MavUI.apk` (artifact) |
| `release.yml` | `v*` tag push'unda üçünü birden derler | **taslak GitHub Release** + üç dosya |

Artifact'ler koşu sayfasında durur, ~90 gün sonra silinir ve indirmek için GitHub hesabı gerekir; kalıcı/paylaşılabilir dosya için `v*` tag'i atıp `release.yml`'in açtığı taslak Release'i yayınla.

Android APK'sının CI'da imzalanması için iki repo secret'i gerekir — yoksa APK imzasız çıkar ve cihaza kurulmaz:

```bash
base64 -w0 ~/Android/mavui.keystore   # -> ANDROID_KEYSTORE_BASE64 secret'i
keytool -list -keystore ~/Android/mavui.keystore   # alias farkliysa ANDROID_KEYSTORE_ALIAS secret'i
# ayrica ANDROID_KEYSTORE_PASSWORD
```

Anahtar yerel `build.sh android` ile aynı olmalı; farklı anahtarla imzalanan APK, tablette kurulu sürümün üzerine güncelleme olarak kurulmaz.

Windows'ta yerel derleme gerekirse aynı yığın kurulur ve `qt-cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` ile derlenir (`build.sh` yalnızca Linux ve Android içindir).

## Araca bağlanma (BlueOS)

Araç ağında (`192.168.2.0/24`) BlueOS `192.168.2.2`'dedir. Masaüstünde `192.168.2.1` alan topside otomatik bağlanır. Tablet/telefon gibi DHCP ile IP alan cihazlarda: **Application Settings → Comm Links** → yeni **UDP** link → hedef `192.168.2.2:14550` (veya BlueOS'ta tanımlı ek `udpin` portu) → Connect.

## Depo düzeni

| Yer | İçerik |
|---|---|
| `custom/` | MavUI overlay'i: markalama, varsayılanlar, Android kimliği/ikonları |
| `main` branch | Varsayılan: sade MavUI + STARS 2026 yarışma eklentileri (**AUV Stars 2026 Mission One** plan üreticisi) |
| `simple` branch | Sade MavUI (yarışma eklentisi yok) — AUV geliştirme ortamı bunu kullanır |
| `torpedo` branch | AuraTorpedo UUV sürümü |
| `custom` branch | Takımın önceki el yazması Qt Widgets GCS'i (arşiv) |

## Lisans ve üst kaynak

Bu proje [QGroundControl](https://github.com/mavlink/qgroundcontrol)'un bir çatallamasıdır ve üst projenin çifte lisansına tabidir: **Apache 2.0** ve **GPLv3** (bkz. `LICENSE-APACHE`, `LICENSE-GPL`). QGroundControl, Dronecode Projesi'nin tescilli markasıdır; bu depo resmi bir QGC dağıtımı değildir.
