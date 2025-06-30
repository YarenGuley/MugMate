# MugMate
# Akıllı Bardak Altlığı 🚰🔥

ESP-32 tabanlı bu akıllı bardak altlığı, kupanızın sıcaklığını anlık olarak gösterir, **saatte bir** su içmenizi hatırlatır ve içeceğinizi bardak altlığında **5 dk** boyunca unutursanız sizi uyarır. 128×32 px OLED, 16 LED’li NeoPixel halka ve titreşim motoru sayesinde sessiz ama etkili bir çok-duyulu geri bildirim sunar.

---

## Özellikler
| Özellik | Açıklama |
|---------|----------|
| **Sıcaklık Takibi** | 10 kΩ NTC termistör ⇒ 35 °C → mavi, 75 °C → kırmızı renk skalası. |
| **OLED Arayüz** | Yüz animasyonu 🙂, dijital saat 🕒, sıcaklık değeri 🌡️ ve bildirim mesajları. |
| **Su Hatırlatıcısı** | Saatte bir “Su içme zamanı!” ⏰ — LED beyaz çift flaş + titreşim (15 s). |
| **İçecek Unutma Uyarısı** | Bardak 5 dk hareket etmezse “İçeceğini unutma” bildirimi. |
| **Sessiz Saat Modu** | 22:00–08:00 arası hatırlatıcı devre dışı. |
| **Kullanıcı İptali** | IR el sensörüyle hatırlatıcıyı erken kapatma. |
| **Nefes Alma Efekti** | Bardak varken LED halkada yumuşak parlaklık soluyuşu. |

---

## Donanım Listesi
| Parça | Adet |
|-------|------|
| ESP-WROOM-32 modülü (DevKitC v4 veya eşdeğer) | 1 |
| WS2812B NeoPixel **16 LED Halka** | 1 |
| 128 × 32 SSD1306 **I²C OLED** | 1 |
| **10 kΩ NTC** termistör + 10 kΩ sabit direnç | 1+1 |
| ESR-10 mm **Titreşim motoru** (3 V) + NPN transistör (2N2222) | 1 |
| IR mesafe sensörü (çift) | 2 |
| 330 Ω seri direnç (NeoPixel DIN) | 1 |
| 1000 µF/6.3 V kondansatör (NeoPixel besleme) | 1 |
| 5 V 2 A adaptör veya USB-C PD (önerilir) | 1 |

---

## Devre Şeması (kısaca)
ESP32 NeoPixel OLED (I²C) Termistör IR sensörler

3V3 ----> VCC VCC <---------- 3V3 VCC
GND ----> GND GND <---------- GND GND
GPIO27 --> DIN NTC ─┐
ADC34 <--┤─*── R_FIXED(10k)── GND
GPIO33 <-- IR1 OUT (bardak) 3V3 ─── NTC ┘
GPIO32 <-- IR2 OUT (el sensörü)
GPIO25 --> NPN Base (titreşim)
SDA/GPIO21 ↔ SDA
SCL/GPIO22 ↔ SCL

---

## Yazılım
* Arduino IDE ≥ 1.8.19  
* Gerekli kütüphaneler  
  * **Adafruit_SSD1306**  
  * **Adafruit_GFX**  
  * **Adafruit_NeoPixel**

### Derleme Ayarları
* **Board:** `ESP32 Dev Module`  
* **Flash Mode:** `QIO`  
* **Flash Size:** `4 MB (32 Mb)`  
* **Partition Scheme:** `Huge APP (3MB No OTA/1MB SPIFFS)`  
* **PSRAM:** `Disabled`

### Program Akışı
setup()
├─ Wi-Fi → NTP ile saat senkronu
├─ Periferikler (OLED, NeoPixel, IR, ADC, Motor) başlat
└─ İlk renk & yüz animasyonu

loop()
├─ Bardak sensörü ⇒ sıcaklık oku, renk güncelle
├─ OLED sahne döngüsü (Yüz → Saat → Sıcaklık)
├─ Su hatırlatıcı zamanlayıcı
│ └─ Uyarı modu: OLED mesaj + 2× LED flaş + titreşim
├─ 5 dk bardak unutma kontrolü
└─ Nefes animasyonu, el sensörü iptali vb.

---

## Kullanım
1. Devreyi şemaya göre kur, 5 V adaptörü tak.  
2. Arduino IDE’de `akilli_bardak_altligi.ino` dosyasını aç → derle → ESP32’ye yükle.  
3. Güç verildiğinde OLED’de yüz animasyonu görünür.  
4. Bardak konulduğunda LED rengi sıcaklığa göre değişir, OLED sıcaklık okur.  
5. Saatte bir su hatırlatıcısı başlar; elini IR sensöre yaklaştırarak susturabilirsin.  

---

## Katkı
Pull request’ler ve hata bildirimleri memnuniyetle karşılanır.  

---

## Lisans
MIT Lisansı – ayrıntı için `LICENSE` dosyasına bakınız.
