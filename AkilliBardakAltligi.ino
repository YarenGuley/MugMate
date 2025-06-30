/************************************************
  AKILLI BARDAK ALTLIGI  v3.1  –  sıcaklık ekranı 1 Hz
************************************************/
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include <BluetoothSerial.h>
#include <WiFi.h>
#include <time.h>

// ---------- PINLER ----------
#define PIN_IR_CUP      33
#define PIN_IR_HAND     32
#define PIN_THERMISTOR  34
#define PIN_NEOPIXEL    27
#define PIN_VIBRATION   25

// ---------- Sabitler ----------
#define NUM_PIXELS    16
Adafruit_NeoPixel ring(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

#define OLED_W 128
#define OLED_H 32
Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);


/********  SU HATIRLATICI DURUM DEĞİŞKENLERİ *******/
/********  SU HATIRLATICI  ********/
bool          reminderActive   = false;        // Hatırlatma döngüsü açık mı?
unsigned long reminderStart    = 0;            // Başlangıç zaman damgası
unsigned long lastReminder     = 0;            // Son otomatik tetikleme
const unsigned long REMINDER_INTERVAL  = 3600000UL;   // 1 saat
const unsigned long REMINDER_DURATION  = 15000UL;     // 15 s
bool          inQuietHours     = false;        // 22:00-08:00 arası
bool          ledsWereOff      = false;        // Hatırlatma bittiğinde LED durumunu geri yükleyebilmek için


unsigned long cupPlacedAt = 0;       // Bardak konulma zamanı  **EKLENDİ**
bool cupReminderSent = false;        // 5 dk uyarısı gönderildi mi?  **EKLENDİ**

// NTC
const float BETA      = 3950.0;
const float T0K       = 298.15;
const float R0        = 10000.0;
const float R_FIXED   = 10000.0;

// zamanlayıcılar
const unsigned long LED_OFF_DELAY     = 60000UL;
const unsigned long TEMP_READ_INT     = 1000UL;
const uint16_t      BREATH_STEP_MS    = 30;
const unsigned long BLINK_LEN_MS      = 100;
const unsigned long GLANCE_LEN_MS     = 1000;

// LED nefes
uint8_t  baseR=0, baseG=0, baseB=255;
uint8_t  breathBright=10;  int8_t breathDir=1;
unsigned long lastBreath=0;

// bardak / sıcak
bool cupPresent=false, prevCup=false, ledsOff=false;
unsigned long cupRemovedAt=0, lastTempRead=0;
float currentTempC=25.0;                 // son okunan sıcaklık

// animasyon
bool eyesOpen=true;   int eyeOffX=0, glanceDir=0;
unsigned long nextBlink=0, blinkEnd=0, nextGlance=0, glanceEnd=0;

// ekran döngüsü
enum Stage{FACE_STAGE,TIME_STAGE,TEMP_STAGE}; Stage stage=FACE_STAGE;
unsigned long stageStart=0;
unsigned long lastTempDisp=0;            // sıcaklık metni ekrana ne zaman son yazıldı

// saat
bool timeValid=false;

// BT / WiFi
BluetoothSerial SerialBT; const char* BT_NAME="AkilliBardakAltligi";
const char* WIFI_SSID="pocox6pro"; const char* WIFI_PASS="123456yaren";
const long GMT_OFFSET=3*3600;

// ---- İLERİ FONKSİYONLAR ---
float  readTempC();
void   computeBaseColor(float);
void   applyBreath();
void   updateFaceAnim();
void   displayFace();

void setup(){
  Serial.begin(115200);
  pinMode(PIN_IR_CUP,INPUT_PULLUP);
  pinMode(PIN_IR_HAND,INPUT_PULLUP);
  pinMode(PIN_VIBRATION,OUTPUT); digitalWrite(PIN_VIBRATION,LOW);

  ring.begin(); ring.clear(); ring.show();

  if(display.begin(SSD1306_SWITCHCAPVCC,0x3C)){ display.clearDisplay(); display.display(); }
  SerialBT.begin(BT_NAME);

  // (Wi-Fi & NTP bölümü öncekiyle aynı – özet)
  if(strlen(WIFI_SSID)){
    WiFi.begin(WIFI_SSID,WIFI_PASS);
    for(uint8_t i=0;i<20 && WiFi.status()!=WL_CONNECTED;i++){ delay(500);}
    if(WiFi.status()==WL_CONNECTED){
      configTime(GMT_OFFSET,0,"pool.ntp.org","time.nist.gov");
      struct tm t; if(getLocalTime(&t,5000)) timeValid=true;
    }
  }

  computeBaseColor(25);
  stageStart = millis();
  unsigned long now = millis();
  nextBlink  = now + random(3000,8000);
  nextGlance = now + random(10000,30000);
}

void loop(){
  unsigned long now = millis();

    // ----- Serial komut kontrolü -----
  if (Serial.available()) {
    static String cmd = "";
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {         // satır bitti
        if (cmd.length() > 0) {
            cmd.trim();                     // varsa \r\n boşluk sil
            if (cmd.equalsIgnoreCase("su")) {
                // Manuel su hatırlatıcısını başlat
                if (!reminderActive) {      // zaten çalışmıyorsa
                    startWaterReminder(millis());
                } else {
                    Serial.println("Hatirlatma zaten aktif.");
                }
            } else {
                Serial.print("Bilinmeyen komut: "); Serial.println(cmd);
            }
            cmd = "";                       // komut tamam, sıfırla
        }
    } else {
        cmd += ch;                          // satır tamponla
        if (cmd.length() > 20) cmd = "";    // aşırı uzun olursa sıfırla
    }
  }

  if (!reminderActive && (millis() - lastReminder >= REMINDER_INTERVAL) && !inQuietHours) {
      startWaterReminder(millis());
  }



  // ---- bardak algıla ----
  cupPresent = digitalRead(PIN_IR_CUP)==LOW;
  if(cupPresent && !prevCup){ ledsOff=false; currentTempC=readTempC(); computeBaseColor(currentTempC); 
      cupPlacedAt = now;               // Konulma zamanını kaydet  **EKLENDİ**
      cupReminderSent = false;         // Yeni bardak için uyarı reset  **EKLENDİ**
  }
  if (SerialBT.hasClient()) {
          SerialBT.println("Bardak alg\u0131land\u0131, afiyet olsun!");  // **EKLENDİ**
      }

  if(!cupPresent && prevCup){ cupRemovedAt=now; cupReminderSent = false;}
  prevCup=cupPresent;

  // ---- LED 1 dk sonra sönsün ----
  if(!cupPresent && !ledsOff && now-cupRemovedAt>=LED_OFF_DELAY){
    ring.clear(); ring.show(); ledsOff=true; Serial.println("LED’ler kapandi");
  }

  // ---- sıcaklık oku ----
  if(cupPresent && now-lastTempRead>=TEMP_READ_INT){
    lastTempRead=now;
    currentTempC = readTempC();
    Serial.printf("Sicaklik: %.1f C\n", currentTempC);
    computeBaseColor(currentTempC);
  }

  // ---- nefes efekti ----
  if(!ledsOff && now-lastBreath>=BREATH_STEP_MS){
    lastBreath=now; breathBright+=breathDir;
    if(breathBright>=250||breathBright<=10) breathDir*=-1;
    applyBreath();
  }

    /*********  SU HATIRLATICI EFEKTİ  *********/
  if (reminderActive)
  {
      unsigned long elapsed = millis() - reminderStart;

      // Flash efekti: 250 ms açık / 250 ms kapalı
      bool flashPhase = (elapsed / 250UL) % 2 == 0;

      // LED'ler
      if (flashPhase) {
          ring.fill(ring.Color(255,255,255));
      } else {
          ring.clear();
      }
      ring.show();

      // OLED (invert yöntemi hızlıdır)
      display.invertDisplay(flashPhase);

      // Titreşim motoru – 250 ms açık / 250 ms kapalı (LED ile senkron)
      digitalWrite(PIN_VIBRATION, flashPhase ? HIGH : LOW);

      // Kapama şartları
      bool handWave = digitalRead(PIN_IR_HAND) == LOW;   // GPIO32 sensörü
      if (handWave || elapsed >= REMINDER_DURATION)
      {
          // Hatırlatmayı bitir
          reminderActive = false;
          display.invertDisplay(false);        // Ekranı normale döndür

          // LED'leri önceki duruma getir
          if (ledsWereOff) {
              ring.clear(); ring.show();
          } else {
              applyBreath();                   // nefes efektli renge geri dön
          }

          digitalWrite(PIN_VIBRATION, LOW);    // motoru kapat
          Serial.println("*** Su hatirlatma BITTI ***");
      }
  }

  

  // ---- ekran DÖNGÜSÜ ----
  uint16_t faceDur=cupPresent?10000:15000, timeDur=cupPresent?10000:15000;
  uint16_t tempDur=cupPresent?10000:0;

  switch(stage){
    case FACE_STAGE:
      updateFaceAnim(); displayFace();
      if(now-stageStart>=faceDur){ stage=TIME_STAGE; stageStart=now; }
      break;

    /**********  ZAMAN ve SICAKLIK EKRAN BLOKLARI – DÜZELTME  **********/

  case TIME_STAGE: {
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);        // <-- EKLENDİ
      display.setCursor(0, 12);

      struct tm t;
      if (timeValid && getLocalTime(&t, 0)) {
          char buf[6]; sprintf(buf, "%02d:%02d", t.tm_hour, t.tm_min);
          display.setTextSize(2);
          display.print(buf);
      } else {
          display.setTextSize(1);
          display.print("saat bilinmiyor!");
      }
      display.display();
      if (now - stageStart >= timeDur) {
          stage = cupPresent ? TEMP_STAGE : FACE_STAGE;
          stageStart = now;
      }
  } break;

  case TEMP_STAGE: {
      if (now - lastTempDisp >= TEMP_READ_INT) {
          lastTempDisp = now;
          display.clearDisplay();
          display.setTextColor(SSD1306_WHITE);    // <-- EKLENDİ
          display.setCursor(0, 12);
          display.setTextSize(2);
          display.printf("%.1f", currentTempC);
          display.setTextSize(1);
          display.print("C");
          display.display();
      }
      if (now - stageStart >= tempDur) {
          stage = FACE_STAGE;
          stageStart = now;
      }
  } break;

    }

    delay(2);
}

/******** yardımcılar ********/
float readTempC() {
  const float VCC = 3.3;           // ADC referans gerilimi
  int adc      = analogRead(PIN_THERMISTOR);   // ham 0-4095
  float Vout   = (adc / 4095.0) * VCC;         // ölçülen gerilim

  float R_NTC = (Vout * R_FIXED) / (VCC - Vout);

  // Beta denklemi → sıcaklık
  float invT  = 1.0/T0K + (1.0/BETA) * log(R_NTC / R0);
  float tempK = 1.0 / invT;
  return tempK - 273.15;           // °C
}

void computeBaseColor(float t){
  if(t<25) t=25; if(t>60) t=60; float f=(t-25)/(60-25);
  baseR=uint8_t(255*f); baseG=0; baseB=uint8_t(255*(1-f));
}
void applyBreath(){
  uint32_t c=ring.Color(baseR*breathBright/255, baseG*breathBright/255, baseB*breathBright/255);
  for(int i=0;i<NUM_PIXELS;i++) ring.setPixelColor(i,c); ring.show();
}
// ---- ANİMASYON ----
void updateFaceAnim(){
  unsigned long now=millis();
  if(now>=nextBlink){ eyesOpen=false; blinkEnd=now+BLINK_LEN_MS; nextBlink=now+random(3000,8000); }
  if(!eyesOpen && now>=blinkEnd){ eyesOpen=true; }
  if(now>=nextGlance){
    glanceDir = random(0,2)?1:-1; eyeOffX=glanceDir*5;
    glanceEnd = now+GLANCE_LEN_MS; nextGlance = now+random(10000,30000);
  }
  if(glanceDir!=0 && now>=glanceEnd){ glanceDir=0; eyeOffX=0; }
}
void displayFace(){
  display.clearDisplay();
  int cx=OLED_W/2, eyeY=10, r=3;
  if(eyesOpen){
    display.fillCircle(cx-20+eyeOffX,eyeY,r,SSD1306_WHITE);
    display.fillCircle(cx+20+eyeOffX,eyeY,r,SSD1306_WHITE);
    display.fillCircle(cx-20+eyeOffX,eyeY,r/2,SSD1306_BLACK);
    display.fillCircle(cx+20+eyeOffX,eyeY,r/2,SSD1306_BLACK);
  }else{
    display.drawLine(cx-23+eyeOffX,eyeY,cx-17+eyeOffX,eyeY,SSD1306_WHITE);
    display.drawLine(cx+17+eyeOffX,eyeY,cx+23+eyeOffX,eyeY,SSD1306_WHITE);
  }
  display.drawLine(cx-15,24,cx+15,24,SSD1306_WHITE);
  display.drawLine(cx-19,22,cx-15,24,SSD1306_WHITE);
  display.drawLine(cx+15,24,cx+19,22,SSD1306_WHITE);
  display.display();
}

void startWaterReminder(unsigned long now)
{
  if (reminderActive) return;        // zaten çalışıyorsa çık

  reminderActive  = true;
  reminderStart   = now;
  lastReminder    = now;             // otomatik sayaç da sıfırlansın

  // Ekranı ilk sefer beyaza çevir
  display.clearDisplay();
  display.fillRect(0, 0, OLED_W, OLED_H, SSD1306_WHITE);
  display.display();

  // LED’lerin önceki durumunu hatırla, beyaz yak
  ledsWereOff = ledsOff;             // (global ledsOff daha önce vardı)
  ring.fill(ring.Color(255,255,255));
  ring.show();

  // BT bildirimi
  if (SerialBT.hasClient())
      SerialBT.println("SU HATIRLATICI: Su icme zamani!");

  Serial.println("*** Su hatirlatma BASLADI ***");
}


