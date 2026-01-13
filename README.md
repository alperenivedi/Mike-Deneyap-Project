# 🤖 Proje MIKE: Yapay Zeka Destekli Fiziksel Asistan

Rabbit R1 projesinden ilham alınarak geliştirilen, **Deneyap Kart** tabanlı akıllı bir yardımcıdır. OpenAI (GPT-4o-mini & Whisper) servisleri ile sesli komutları metne çevirir, akıllı yanıtlar üretir ve daktilo efekti ile ekrana yansıtır.

## 📺 Proje Tanıtım ve Demo Videosu
Aşağıdaki görsele tıklayarak Mike'ın tüm özelliklerini, menü geçişlerini ve yapay zeka ile olan sohbetini izleyebilirsiniz:

[![Proje MIKE Tanıtım Videosu](https://img.youtube.com/vi/7PUb-RcAiwI/0.jpg)](https://www.youtube.com/watch?v=7PUb-RcAiwI)

---

## ✨ Temel Özellikler
* **Akıllı Sohbet:** Whisper (STT) ile ses kaydı ve GPT-4o-mini (LLM) ile hızlı, mantıklı yanıtlar.
* **Ortam Takibi:** SHT4x ve Basınç sensörleri ile oda sıcaklığı, nem oranı ve hava basıncı (hPa) ölçümü.
* **Bilgi Servisleri:** Open-Meteo API ile Bilecik lokasyonu için 5 günlük animasyonlu hava durumu tahmini.
* **Zaman Yönetimi:** NTP üzerinden senkronize edilen modern dijital ve analog saat/takvim.
* **Verimlilik Araçları:** Ayarlanabilir Pomodoro sayacı ve süre bittiğinde devreye giren sesli alarm sistemi.
* **Donanım Kontrolü:** NeoPixel RGB LED üzerinden renk geçişleri ve özel "Disko Modu".

## 🛠 Donanım Bileşenleri
| Malzeme | Görevi |
| :--- | :--- |
| **Deneyap Kart 1A v2** | Projenin ana işlemcisi ve Wi-Fi kontrol ünitesi. |
| **INMP441 Mikrofon** | I2S protokolü ile çalışan yüksek hassasiyetli mikrofon. |
| **Deneyap OLED Ekran** | Kullanıcı arayüzü ve göz kırpma animasyonları. |
| **Deneyap Hoparlör** | Sesli yanıtlar, daktilo efektleri ve alarm tonları. |
| **Sensör Modülleri** | Sıcaklık, Nem ve Basınç ölçüm birimleri. |
| **Güç Kaynağı** | 1800 mAh Li-Polymer Pil ile taşınabilir kullanım. |

## 🕹 Kullanıcı Etkileşimi
Mike, sezgisel bir kontrol şemasına sahiptir:
* **Potansiyometre:** Menüler arası geçiş, uzun metinleri kaydırma ve Pomodoro süresini ayarlama.
* **Buton Fonksiyonları:**
    * **Tek Tık:** Seçim yapar veya Mike'ı dinleme moduna sokar.
    * **Çift Tık:** Mevcut uygulamadan çıkar veya ana menüye döner.
    * **Uzun Basış (3 sn):** Pomodoro ayar moduna girer veya süreyi kaydeder.

* **Adafruit_SSD1306 ve Adafruit_GFX

* **ArduinoJson 

* **Adafruit_SHT4x ve Deneyap_BasincOlcer

* **Adafruit_NeoPixel 

---
