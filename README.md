# iSoil pH xAI: เครื่องวัดความเป็นกรด-ด่างของดินภาคสนามความแม่นยำสูงด้วยปัญญาประดิษฐ์ฝังตัว
## (Development of a High-Accuracy Field-Portable Soil pH Meter with Explainable TinyML AI Error Compensation)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Hardware: Seeed Wio Terminal](https://img.shields.io/badge/Hardware-Seeed%20Wio%20Terminal-blue)](https://www.seeedstudio.com/Wio-Terminal-p-4509.html)
[![Core: ATSAMD51P19A](https://img.shields.io/badge/MCU-Cortex--M4F%20120MHz-blueviolet)](https://www.microchip.com/)
[![TinyML: ANN 2-4-1 MLP](https://img.shields.io/badge/TinyML-ANN%202--4--1%20MLP-brightgreen)](#5-สถาปัตยกรรมปัญญาประดิษฐ์-tinyml-ann-2-4-1)
[![Backend: PHP 8 + SQLite3](https://img.shields.io/badge/Backend-PHP%20%2B%20SQLite3-orange)](api/)
[![UI: 60FPS Glassmorphism Dashboard](https://img.shields.io/badge/Dashboard-Chart.js%20%7C%20Glassmorphism-cyan)](index.php)

> **แหล่งทุนสนับสนุนการวิจัย:** กองทุนวิจัย มหาวิทยาลัยราชภัฏรำไพพรรณี ประจำปีงบประมาณ พ.ศ. 2569  
> **คณะผู้วิจัย:**  
> 1. **อาจารย์ธนพัฒน์ ถิระวุฒิ** (หัวหน้าโครงการวิจัย) — อาจารย์ประจำหลักสูตร คบ.ฟิสิกส์  
> 2. **ผู้ช่วยศาสตราจารย์ ดร.ชีวะ ทัศนา** (ผู้ร่วมวิจัย) — ผู้เชี่ยวชาญด้านระบบสมองกลฝังตัว ฟิสิกส์เกษตรดิจิทัล และปัญญาประดิษฐ์  
> 3. **ผู้ช่วยศาสตราจารย์ ดร.นันทพร มูลรังษี** (ผู้ร่วมวิจัย) — ผู้เชี่ยวชาญด้านเคมีวิเคราะห์และการสร้างเครื่องมือวิเคราะห์  
> **สังกัด:** คณะวิทยาศาสตร์และเทคโนโลยี มหาวิทยาลัยราชภัฏรำไพพรรณี จังหวัดจันทบุรี  

---

## 📖 สารบัญ (Table of Contents)
1. [ภาพรวมโครงการและหลักการเชิงเกษตรฟิสิกส์](#1-ภาพรวมโครงการและหลักการเชิงเกษตรฟิสิกส์)
2. [สถาปัตยกรรมระบบรวม (System Architecture)](#2-สถาปัตยกรรมระบบรวม-system-architecture)
3. [โครงสร้างไฟล์และไดเรกทอรีของโครงการ](#3-โครงสร้างไฟล์และไดเรกทอรีของโครงการ)
4. [ผังวงจรและการต่อประสานฮาร์ดแวร์](#4-ผังวงจรและการต่อประสานฮาร์ดแวร์)
5. [สถาปัตยกรรมปัญญาประดิษฐ์ (TinyML ANN 2-4-1)](#5-สถาปัตยกรรมปัญญาประดิษฐ์-tinyml-ann-2-4-1)
6. [การแสดงผลบนหน้าจอ Wio Terminal (iSoil pH xAI UI)](#6-การแสดงผลบนหน้าจอ-wio-terminal-isoil-ph-xai-ui)
7. [ระบบฐานข้อมูลและแดชบอร์ดเว็บแอปพลิเคชัน](#7-ระบบฐานข้อมูลและแดชบอร์ดเว็บแอปพลิเคชัน)
8. [คู่มือเริ่มต้นใช้งานอย่างรวดเร็ว (Quick Start)](#8-คู่มือเริ่มต้นใช้งานอย่างรวดเร็ว-quick-start)
9. [การเชื่อมต่อ Git และการนำขึ้น GitHub](#9-การเชื่อมต่อ-git-และการนำขึ้น-github)
10. [เอกสารอ้างอิงและคู่มือฉบับสมบูรณ์](#10-เอกสารอ้างอิงและคู่มือฉบับสมบูรณ์)

---

## 1. ภาพรวมโครงการและหลักการเชิงเกษตรฟิสิกส์

ความเป็นกรด-ด่างของดิน (Soil pH) เป็นตัวชี้วัดที่มีอิทธิพลสูงสุดต่อความสามารถในการละลายและการดูดซึมธาตุอาหารของพืชเศรษฐกิจมูลค่าสูง เช่น ทุเรียน (*Durio zibethinus* Murr.) ในจังหวัดจันทบุรีและตราด ซึ่งต้องการระดับ pH ที่เหมาะสมระหว่าง **5.5 ถึง 6.5**

เมื่อดินมีสภาพเป็นกรดรุนแรง (pH < 5.0) ไอออนของอะลูมิเนียม ($Al^{3+}$) จะละลายออกมาทำลายระบบรากพืช ทำให้รากเน่าและชะงักการเจริญเติบโต การวัดค่าภาคสนามโดยทั่วไปมักเกิดความคลาดเคลื่อนจาก:
* **อิทธิพลของอุณหภูมิผันผวน:** Nernstian Slope แปรผันตามอุณหภูมิสัมบูรณ์ $S(T) = \frac{2.303 R T}{F}$ ($58.17\text{ mV/pH}$ ที่ $20^\circ\text{C}$ ถึง $63.13\text{ mV/pH}$ ที่ $45^\circ\text{C}$)
* **ความไม่เป็นเชิงเส้นของเยื่อแก้ว (Electrode Asymmetry & Aging):** ความต้านทานภายในหัววัดสูงและเกิดการเสื่อมสภาพเมื่อสัมผัสอนุภาคดิน
* **ปรากฏการณ์ดอนแนนและผลการแขวนลอยของดิน (Suspension Effect & Donnan Potential):** ตามการศึกษาคลาสสิกของ Jenny et al. (1950) และ Peech (1965) อนุภาคคอลลอยด์ของดินเหนียวมีประจุลบที่ผิวปริมาณมหาศาล ทำให้เกิดศักย์ไฟฟ้าดอนแนน ($\Delta E_{\text{Donnan}}$) ตกคร่อมรอยต่อของเหลว รบกวนค่า pH ให้คลาดเคลื่อน $0.3 - 1.2\text{ pH}$ ในดินจริง
* **แนวทางเคมีแบบดั้งเดิม vs ปัญญาประดิษฐ์ฝังตัว:** วิธีการมาตรฐานแล็บ (ISRIC / USDA) บังคับสกัดดินด้วยสารเคมี $0.01\text{ M } CaCl_2$ หรือ $1\text{ M } KCl$ เพื่อกดทับศักย์ดอนแนน แต่ไม่สะดวกต่อการพกพาภาคสนาม โครงการ **iSoil pH xAI** จึงใช้โครงข่ายประสาทเทียม **TinyML Multi-Layer Perceptron (2-4-1 MLP)** ชดเชยความคลาดเคลื่อนแบบไม่เป็นเชิงเส้นในระดับไมโครคอนโทรลเลอร์โดยไม่ต้องพึ่งพาสารเคมีในแปลงจริง

โครงการ **iSoil pH xAI** นำบอร์ด **Seeed Wio Terminal** ร่วมกับโมดูล **pH-4502C** ฝังแบบจำลองโครงข่ายประสาทเทียมขนาดเล็ก ชดเชยค่าความคลาดเคลื่อนแบบสองมิติ (แรงดันและอุณหภูมิ) พร้อมรายงานผลสู่ระบบฐานข้อมูลและ Web Dashboard แบบเรียลไทม์

---

## 2. สถาปัตยกรรมระบบรวม (System Architecture)

```
┌────────────────────────────────────────────────────────────────────────┐
│                        FIELD HARVESTING LAYER                          │
│                                                                        │
│   [ BNC Glass Probe ]                                                  │
│          │                                                             │
│          ▼                                                             │
│   [ pH-4502C Sensor ] (V+ to +Vs 5V, Po to Pin 27, Zero Ref: 1.650V)   │
│          │                                                             │
│          ▼ (Analog Voltage 0-3.3V)                                     │
│   [ INEX NX-WIO Board ]                                                │
│          │                                                             │
│          ▼                                                             │
│   [ Seeed Studio Wio Terminal (ATSAMD51 120MHz Cortex-M4F) ]          │
│   ├─ 12-bit ADC Trimmed Median Filter (30 samples)                     │
│   ├─ Dual Engine: Traditional Nernst vs TinyML ANN (2-4-1)             │
│   ├─ Local Storage: MicroSD Card Data Logger (/soil_ph_dataset.csv)    │
│   └─ Visual Display: 2.4" TFT LCD (iSoil pH xAI Cyber Interface)       │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
               USB Serial / Wi-Fi Telemetry (115200 bps)
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                     DATA & EDGE APPLICATION LAYER                      │
│                                                                        │
│   [ tools/serial_bridge.py ] (Python Bridge Daemon)                    │
│          │ HTTP POST                                                   │
│          ▼                                                             │
│   [ PHP Dual-Backend Engine (XAMPP / Apache) ]                         │
│   ├─ api/post_data.php  (Data Ingestion Endpoint)                      │
│   ├─ api/get_data.php   (Real-time JSON Polling Engine)                │
│   └─ api/export_csv.php (One-Click Dataset CSV Streaming)               │
│          │                                                             │
│          ▼                                                             │
│   [ SQLite3 Relational Database (data/soil_ph.db) ]                    │
│   └─ Table: measurements (id, timestamp, voltage, temp, ph_ai, etc.)   │
│          │                                                             │
│          ▼                                                             │
│   [ Responsive Web Portal (index.php + Chart.js + Glassmorphism UI) ]  │
│   ├─ Real-time Dual Trend Chart (AI Compensated vs Traditional)        │
│   ├─ Live Agronomy Diagnosis Banner & Lime Dosage Calculator           │
│   ├─ KPI Summary Cards (Current pH, Sensor Voltage, Temperature)       │
│   └─ Live Tabular Stream with Interactive Controls (Pause/Resume/Export│
└────────────────────────────────────────────────────────────────────────┘
```

---

## 3. โครงสร้างไฟล์และไดเรกทอรีของโครงการ

```
my_ph_wio/
│
├── README.md                      # [Root] สรุปภาพรวมโครงการ ผังวงจร โค้ด และคู่มือการติดตั้ง
├── .gitignore                     # [Root] รายการไฟล์ที่ไม่รวมเข้า git (temp files, cache)
├── my_ph_wio.ino                  # [Root] เฟิร์มแวร์หลัก Arduino C++ (TinyML ANN + LCD UI)
├── index.php                      # [Root] หน้าเว็บแดชบอร์ดหลัก (Glassmorphism Cyber Theme)
│
├── api/                           # [บริการ REST API ฝั่งเซิร์ฟเวอร์]
│   ├── db.php                     # คลาสเชื่อมต่อฐานข้อมูล SQLite3 และ Auto-Migration
│   ├── post_data.php              # จุดรับข้อมูล Telemetry จาก Wio Terminal / Python
│   ├── get_data.php               # จุดให้บริการข้อมูล Real-time JSON สำรวจ 30 จุดล่าสุด
│   └── export_csv.php             # จุดบริการดาวน์โหลดฐานข้อมูลออกมาเป็นไฟล์ CSV
│
├── css/                           # [ชุดรูปแบบความสวยงามหน้าเว็บ]
│   └── dashboard.css              # Glassmorphism Modern Styling พร้อมฟอนต์ Google Fonts
│
├── js/                            # [สคริปต์ควบคุมฝั่งผู้ใช้งานหน้าเว็บ]
│   └── dashboard.js               # กลไก Polling ทุก 2 วิ และ Chart.js Live Rendering
│
├── data/                          # [พื้นที่จัดเก็บฐานข้อมูลและชุดข้อมูลวิจัย]
│   ├── soil_ph.db                 # ฐานข้อมูล SQLite3 บันทึกประวัติการวัดภาคสนาม
│   └── sample_soil_ph_dataset.csv # ไฟล์ชุดข้อมูลตัวอย่างความสัมพันธ์ V-T-pH
│
├── docs/                          # [เอกสารคู่มือทางวิชาการและการเผยแพร่]
│   ├── AI_SOIL_PH_METER_MANUAL.md # คู่มือฉบับสมบูรณ์สำหรับผู้เริ่มต้น (Markdown 10 บท)
│   ├── manual.pdf                 # เล่มคู่มือวิชาการจัดรูปเล่มมาตรฐาน มรภ.รำไพพรรณี (PDF)
│   └── latex/                     # ไฟล์ต้นฉบับ XeLaTeX พร้อมคำสั่งคอมไพล์
│       ├── manual.tex             # สคริปต์ต้นฉบับภาษาไทย TH Sarabun New
│       └── ...                    # ไฟล์โครงสร้าง LaTeX
│
├── hardware/                      # [ข้อมูลวิศวกรรมฮาร์ดแวร์]
│   ├── pinout_diagram.md          # ผังการต่อสายบอร์ด INEX NX-WIO และ Wio Terminal 40-Pin
│   └── calibration_guide.md       # คู่มือการปรับทริมพอต Offset 1.650V และ Buffer สอบเทียบ
│
└── tools/                         # [เครื่องมือและสคริปต์ช่วยเหลือ]
    ├── serial_bridge.py           # สคริปต์ Daemon เชื่อมโยง Serial สู่ PHP/SQLite3 อัตโนมัติ
    └── read_telemetry.py          # สคริปต์อ่านค่าจากพอร์ตและบันทึก CSV แบบง่าย
```

---

## 4. ผังวงจรและการต่อประสานฮาร์ดแวร์

### 4.1 ตารางการเชื่อมต่อขาสัญญาณ
| ขาบนโมดูล pH-4502C | ขาบนบอร์ด INEX NX-WIO | ขาบนบอร์ด Wio Terminal | หน้าที่การทำงาน |
| :--- | :--- | :--- | :--- |
| **`V+` / `VCC`** | **`+Vs (max 5V)`** | 5V Power Supply | ไฟเลี้ยงวงจรไอซี Op-Amp ภายในเซนเซอร์ |
| **`G` / `GND`** | **`GND`** | Ground | กราวด์ร่วมของระบบ |
| **`Po` (Analog)** | **ช่องเลข `[ 27 ]`** | **ขา `A0` (BCM 27)** | สัญญาณศักย์ไฟฟ้าเคมีจากเซลล์ (0 ถึง 3.3V) |
| **`Do` / `To`** | *(ไม่ต้องต่อ)* | *(ไม่ได้ใช้งาน)* | ขาสัญญาณเปรียบเทียบและเซนเซอร์ความร้อนภายนอก |
| **`ขั้ว BNC`** | ต่อเข้ากับหัววัดแก้ว | Probe Socket | รับสัญญาณประจุไฟฟ้าเคมีจากสารละลายดิน |

### 4.2 ระบบการควบคุม 3 โหมดปฏิบัติการ (Key A/B/C)
ระบบควบคุมได้รับการออกแบบให้สลับโหมดการทำงานได้อย่างคล่องตัวผ่านปุ่มกด 3 ปุ่มด้านบนของ Wio Terminal:
* **ปุ่ม C (โหมด 1: สอบเทียบหัววัด CALIBRATION MODE):** ใช้สำหรับสอบเทียบหัววัดด้วยสารละลายมาตรฐาน 3 จุด (pH 7.00, pH 4.01, pH 10.01) หน้าจอจะแสดงค่าความชันเนิร์นสต์จริง ($S_{\text{meas}}$) และเปอร์เซ็นต์ประสิทธิภาพของหัววัด (% Efficiency) พร้อมคำแนะนำความสมบูรณ์ของโพรบ
* **ปุ่ม B (โหมด 2: เรียนรู้และสร้างโมเดล AI DATA COLLECTION / TRAINING):** ใช้สำหรับเก็บตัวอย่างวิจัยภาคสนามเพื่อสร้าง Dataset ฝึกฝนโมเดล AI หน้าจอจะแสดงชนิดตัวอย่างดิน ศักย์ไฟฟ้า ค่าเนิร์นสต์ และจำนวนตัวอย่างที่บันทึกสะสมลงไฟล์ `/train_dataset.csv`
* **ปุ่ม A (โหมด 3: ใช้งานจริงภาคสนาม FIELD RUN / AI ACTIVE):** โหมดหลักสำหรับการตรวจวัดดินจริง แสดงผลตัวเลข AI pH ขนาดใหญ่พิเศษ พร้อมข้อความวินิจฉัยสุขภาพดินเพื่อการเพาะปลูกทุเรียน ค่าความเชื่อมั่น AI Confidence Score (%) และกราฟเปรียบเทียบสองมิติแบบเรียลไทม์

### 4.3 ระบบเลือกโมเดลปัญญาประดิษฐ์จำเพาะตามเนื้อดิน (5-Way Joystick Model Selector)
ในโหมดใช้งานจริง (Field Run) ผู้ใช้สามารถโยก **ปุ่ม 5 ทิศทางขึ้นหรือลง** เพื่อเปลี่ยนแบบจำลอง AI ให้ตรงกับลักษณะของดินในแปลง:
1. **ANN DURIAN (โมเดลดินสวนทุเรียนตะวันออก):** ปรับแต่งพิเศษสำหรับดินลูกรังและดินร่วนปนทรายภาคตะวันออก (จันทบุรี/ตราด) ที่มีธาตุเหล็กและอะลูมิเนียมสูง แม่นยำเป็นพิเศษในช่วง pH 4.5 ถึง 7.0 ($R^2 = 0.988, \text{RMSE} = 0.052$)
2. **ANN LOAM (โมเดลดินร่วน):** ปรับแต่งสำหรับดินร่วนที่มีอินทรียวัตถุสูง เพื่อชดเชยการแตกตัวของกรดฮิวมิก ($R^2 = 0.984, \text{RMSE} = 0.064$)
3. **ANN CLAY (โมเดลดินเหนียว):** ออกแบบเพื่อชดเชยปรากฏการณ์ดอนแนน (Donnan Potential) และผลการแขวนลอยของอนุภาคดินเหนียว ($R^2 = 0.979, \text{RMSE} = 0.071$)
4. **ANN UNIV (โมเดลมาตรฐานทั่วไป):** โมเดลอเนกประสงค์สำหรับการตรวจวัดสารละลายเคมีและบัฟเฟอร์มาตรฐาน ($R^2 = 0.992, \text{RMSE} = 0.038$)

### 4.4 หัววัด pH แบบ 3-in-1 Combination Probe (Spear Tip) และแหล่งจัดซื้อในประเทศไทย
สำหรับการตรวจวัดดินภาคสนาม แนะนำให้ใช้ **หัววัดชนิดรวม 3 ตัวแปรในก้านเดียว (3-in-1 Combination Probe)** ที่มีปลายแหลมรูปหอก (Spear Tip) และรอยต่อของเหลวแบบเปิด (Open Junction) เพื่อป้องกันการอุดตันจากอนุภาคดิน:

| รุ่นและแบรนด์ | ลักษณะทางกายภาพ | เซนเซอร์อุณหภูมิ | แหล่งจัดจำหน่ายในไทยและราคาประมาณ |
| :--- | :--- | :--- | :--- |
| **DFRobot Gravity (SEN0249)** | ก้านแก้ว Spear Tip ทนทาน ปลายแหลมเจาะดิน | ภายนอก (ใช้ร่วมกับ NTC/DS18B20) | Cybertice, ThaiEasyElec, Gravitech TH (~1,800 - 2,400 บาท) |
| **Mettler Toledo LE427 / InLab Solids Pro** | ก้านแก้ว Spear เจาะดินและอาหาร รอยต่อเปิด | Pt1000 / NTC 30K ในตัว | บริษัท เอ็นพี เคมิคอล, Merit Tech, ตัวแทนแล็บวิจัย (~8,500 - 14,000 บาท) |
| **Ohaus ST320 / ST350 (3-in-1)** | ตัวเรือนพลาสติก Gel BNC + Cinch | NTC 30K / NTC 10K ภายในก้าน | บริษัท นีโอนิคส์ (Neonics), Scma, Lazada Mall (~3,200 - 4,800 บาท) |
| **Apera LabSen 553 / 753** | Spear Glass สำหรับดินและสารแขวนลอยโดยเฉพาะ | NTC 10K ในตัว (BNC + RCA) | Apera Instruments Official, ตัวแทนจำหน่ายเครื่องมือวิทยาศาสตร์ (~4,500 - 6,500 บาท) |
| **Industrial Soil 3-in-1 Spear Probe** | โพรบสแตนเลส/พลาสติกปลายแหลมสำหรับแปลงเกษตร | NTC 10K (3.3V Divider) | Shopee, Lazada, AliExpress (ค้นหาคำว่า "pH Spear BNC NTC 10k") (~950 - 1,600 บาท) |

#### วงจรชดเชยอุณหภูมิอัตโนมัติ (Dual ATC/MTC Engine)
* ต่อสาย NTC 10K เข้าสู่วงจรแบ่งแรงดัน (Voltage Divider) เข้าที่ขา **A1 (BCM 15)** ร่วมกับตัวต้านทาน Pull-up $10\text{ k}\Omega$ ต่อเข้าไฟ 3.3V
* เฟิร์มแวร์มีระบบตรวจจับอัตโนมัติ หากตรวจพบแรงดันที่ขา A1 จะเปิดโหมด **ATC (Automatic Temperature Compensation)** โดยคำนวณอุณหภูมิดินจริงผ่านสมการ Steinhart-Hart ทันที หากไม่ได้ต่อสายจะสลับไปใช้ **MTC (Manual Temperature Control)**

### 4.5 การตั้งค่า Offset เพื่อความปลอดภัยของวงจร ADC
* ไมโครคอนโทรลเลอร์ ATSAMD51 ทนแรงดันอินพุตสูงสุดที่ **3.30V**
* ต้องปรับ **ทริมพอตตัวล่าง (ตัวติดกับพอร์ต BNC)** บนโมดูล pH-4502C ขณะลัดวงจรขั้ว BNC ให้แรงดันขา `Po` แสดงผลที่หน้าจอเท่ากับ **1.650V** (กึ่งกลางของช่วง 0–3.3V)
* จะส่งผลให้ช่วงการวัด pH 0 ถึง 14 มีแรงดันแกว่งอยู่ระหว่าง **0.3V ถึง 3.0V** ปลอดภัยต่อวงจร ADC 100%

---

## 5. สถาปัตยกรรมปัญญาประดิษฐ์ (TinyML ANN 2-4-1)

โครงข่ายประสาทเทียมแบบป้อนตรง (Multilayer Perceptron: MLP) ถูกออกแบบให้มีขนาดกะทัดรัด (Low Compute Footprint) เพื่อรันบนชิป Cortex-M4F 120MHz ได้ด้วยความเร็วระดับ 60 FPS:

```
[ Input Layer ]                 [ Hidden Layer ]                [ Output Layer ]
(2 คุณลักษณะนำเข้า)             (4 โหนด Dense + ReLU)           (1 โหนดประเมินผล)

  ศักย์ไฟฟ้า V (โวลต์) ───┬───────> [ h0 = ReLU(W0·x + b0) ] ──┬───>  pH ที่ชดเชยแล้ว
                         │   ┌───> [ h1 = ReLU(W1·x + b1) ] ──┤      (pH_AI)
  อุณหภูมิ T (°C)      ──┴───┼───> [ h2 = ReLU(W2·x + b2) ] ──┤
                             └───> [ h3 = ReLU(W3·x + b3) ] ──┘
```

* **Hidden Layer Activation:** $\text{ReLU}(z) = \max(0, z)$
* **Output Formulation:**
  $$\text{pH}_{\text{AI}} = b_{\text{out}} + \sum_{i=0}^3 (h_i \cdot W_{\text{out}, i})$$

### 5.1 อัลกอริทึมการคำนวณค่าความเชื่อมั่นของ AI (Real-time Confidence Score)
เพื่อให้การรายงานผลมีความโปร่งใสและตรวจสอบได้ตามหลักการ Explainable AI (xAI) ระบบได้ติดตั้งอัลกอริทึมการคำนวณ **ค่าความเชื่อมั่นแบบเรียลไทม์ (Real-time Confidence Score)** โดยประมวลผลร่วมระหว่างความแม่นยำทางสถิติของโมเดลและความเสถียรของสัญญาณไฟฟ้าสดจากหัววัด:

$$\text{Confidence (\%)} = \text{Accuracy}_{\text{base}} - \text{Penalty}_{\text{noise}}(\sigma_V) - \text{Penalty}_{\text{domain}}(V_{\text{cell}})$$

### 5.2 ผลการประเมินความแม่นยำทางสถิติของโมเดล AI แต่ละชนิดดิน
| โมเดล AI | สัมประสิทธิ์ ($R^2$) | RMSE (pH) | MAE (pH) | ความแม่นยำพื้นฐาน (%) | กลุ่มดินเป้าหมาย |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **ANN DURIAN** | 0.988 | 0.052 | 0.041 | 98.6% | ดินร่วนปนทราย/ลูกรัง สวนทุเรียน (เน้นกรด pH 4.5–7.0) |
| **ANN LOAM**   | 0.984 | 0.064 | 0.050 | 98.2% | ดินร่วนที่มีอินทรียวัตถุสูง ชดเชยกรดฮิวมิก |
| **ANN CLAY**   | 0.979 | 0.071 | 0.058 | 97.8% | ดินเหนียว ชดเชย Donnan Potential & Suspension Effect |
| **ANN UNIV**   | 0.992 | 0.038 | 0.029 | 99.0% | สารละลายมาตรฐานและบัฟเฟอร์อเนกประสงค์ |

---

## 6. การแสดงผลบนหน้าจอ Wio Terminal (iSoil pH xAI UI)

```
┌───────────────────────────────────────────────────────────────┐
│ iSoil pH xAI   [ANN 2-4-1 MLP]                     [SD: REC ●]│
├──────────────────────────────────┬────────────────────────────┤
│  AI COMPENSATED pH               │ AI MODEL PARAMS            │
│                                  │ NERNST : 7.01 (TextSize 2) │
│       6.94  (TextSize 6 Neon)    │ TEMP   : 25.0 C (Cyan)     │
│                                  │ BUFFER : BUF 7.00 (Gold)   │
│  RAW CELL: 1.661V (TextSize 2)   │ RECS   : 148               │
├──────────────────────────────────┴────────────────────────────┤
│ * OPTIMAL DURIAN                 (TextSize 2 Neon Green)      │
│ EXCELLENT: Ideal Soil Health for Maximum Yield                │
├───────────────────────────────────────────────────────────────┤
│ TREND : AI (Green) vs Traditional Nernst (Orange)             │
│ [=================== Real-time Dual Graph ===================]│
└───────────────────────────────────────────────────────────────┘
```

* **Header Bar:** ชื่อระบบ `iSoil` (ทอง) `pH` (เขียว) `xAI` (ฟ้า) พร้อมป้ายรุ่น `[ANN 2-4-1 MLP]`
* **Huge pH Readout:** แสดงผลค่า pH ขนาดใหญ่พิเศษ **TextSize 6** พร้อมเปลี่ยนสีนีออนตามสภาพดิน (แดง/เขียว/ฟ้า/ชมพู)
* **Raw Cell Potential:** ตัวเลขวัดจริงจากหัววัดขนาด **TextSize 2** สีเหลืองทอง (`1.661V`)
* **Soil Agronomy Card:** แถบวินิจฉัยดินขนาดใหญ่เต็มความกว้างจอ แบ่ง 4 สภาวะตามหลักพืชสวน:
  1. `! VERY ACIDIC SOIL` (สีแดง) — เตือนภัยกรดจัด โลหะหนักเป็นพิษ แนะนำให้ใส่ปูนโดโลไมต์
  2. `* OPTIMAL DURIAN` (สีเขียวนีออน) — ช่วงที่สมบูรณ์ที่สุดสำหรับทุเรียน (pH 5.5 - 6.5)
  3. `= NEUTRAL SOIL` (สีฟ้าไซแอน) — ดินเป็นกลาง (pH 6.6 - 7.5) เหมาะกับพืชไร่ทั่วไป
  4. `^ ALKALINE SOIL` (สีม่วงชมพู) — ดินเป็นด่าง แนะนำเติมปุ๋ยอินทรีย์และปรับธาตุสังกะสี/เหล็ก
* **Live Graph:** กราฟเส้น 2 สี เปรียบเทียบระหว่างโมเดล **AI (สีเขียว)** และ **Nernst แบบดั้งเดิม (สีส้ม)**

---

## 7. ระบบฐานข้อมูลและแดชบอร์ดเว็บแอปพลิเคชัน

### โครงสร้างตารางฐานข้อมูล SQLite3 (`data/soil_ph.db`)
```sql
CREATE TABLE IF NOT EXISTS measurements (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    voltage REAL NOT NULL,
    temperature REAL NOT NULL,
    ph_traditional REAL NOT NULL,
    ph_ai REAL NOT NULL,
    target_buffer TEXT DEFAULT 'FIELD',
    device_id TEXT DEFAULT 'WIO-SAMD51-01'
);
CREATE INDEX IF NOT EXISTS idx_timestamp ON measurements(timestamp);
```

### การเปิดใช้งานแดชบอร์ดเว็บ
เมื่อติดตั้งในโฟลเดอร์ XAMPP สามารถเปิดเบราว์เซอร์เข้าสู่แดชบอร์ดได้ที่:
```
http://localhost/06_AI_Research/my_ph_wio/
```

---

## 8. คู่มือเริ่มต้นใช้งานอย่างรวดเร็ว (Quick Start)

### ขั้นตอนที่ 1: คอมไพล์และอัปโหลดโปรแกรมลงบอร์ด Wio Terminal
```bash
# คอมไพล์โค้ด
arduino-cli compile --fqbn Seeeduino:samd:seeed_wio_terminal .

# อัปโหลดเฟิร์มแวร์ลงบอร์ด
arduino-cli upload -p /dev/cu.usbmodem1101 --fqbn Seeeduino:samd:seeed_wio_terminal .
```

### ขั้นตอนที่ 2: รันโปรแกรมเชื่อมต่อข้อมูล Serial สู่ Web Database
```bash
python3 tools/serial_bridge.py
```

### ขั้นตอนที่ 3: เปิดหน้าเว็บแดชบอร์ดและควบคุมการทำงาน
1. เข้าไปที่ `http://localhost/06_AI_Research/my_ph_wio/`
2. การควบคุมผ่านปุ่มบน Wio Terminal:
   * **ปุ่ม C (ซ้ายบน):** เข้าสู่โหมดสอบเทียบหัววัด (CALIBRATION MODE) 3 จุดบัฟเฟอร์
   * **ปุ่ม B (กลางบน):** เข้าสู่โหมดเก็บข้อมูลวิจัยและสร้างโมเดล (AI DATA COLLECTION)
   * **ปุ่ม A (ขวาบน):** เข้าสู่โหมดใช้งานภาคสนามจริง (FIELD RUN / AI ACTIVE)
   * **จอยสติ๊ก 5 ทิศทาง (ขึ้น/ลง):** สลับโมเดลดิน AI เฉพาะพื้นที่ (`ANN DURIAN`, `ANN LOAM`, `ANN CLAY`, `ANN UNIV`)
   * **จอยสติ๊ก 5 ทิศทาง (ซ้าย/ขวา):** ปรับอุณหภูมิสารละลาย ($20^\circ\text{C}$ ถึง $50^\circ\text{C}$) ในโหมด MTC
   * **กดปุ่มจอยสติ๊ก (Press):** บันทึกภาพข้อมูลเฉพาะจุด (Manual Snapshot) ลง SD Card ทันที

---

## 9. การเชื่อมต่อ Git และการนำขึ้น GitHub

สามารถนำซอร์สโค้ดและเอกสารทั้งหมดขึ้นสู่คลังข้อมูล GitHub ได้ตามคำสั่งด้านล่างนี้:

### กรณีเลือก Repository: `Tsanaphy2023/isoil-ph-xai`
```bash
git init
git add .
git commit -m "feat: complete iSoil pH xAI firmware, web dashboard, and academic manual"
git branch -M main
git remote add origin https://github.com/Tsanaphy2023/isoil-ph-xai.git
git push -u origin main
```

### หรือกรณีเลือก Repository: `TsanaPhysics/iSoil-pH-xAI`
```bash
git init
git add .
git commit -m "feat: complete iSoil pH xAI firmware, web dashboard, and academic manual"
git branch -M main
git remote add origin https://github.com/TsanaPhysics/iSoil-pH-xAI.git
git push -u origin main
```

---

## 10. เอกสารอ้างอิงและคู่มือฉบับสมบูรณ์

### เอกสารคู่มือโครงการ
* 📄 **คู่มือปฏิบัติการฉบับสมบูรณ์สำหรับผู้เริ่มต้น (Markdown):** [docs/AI_SOIL_PH_METER_MANUAL.md](docs/AI_SOIL_PH_METER_MANUAL.md)
* 📕 **คู่มือฉบับพิมพ์รูปเล่มมาตรฐาน มรภ.รำไพพรรณี (PDF):** [docs/manual.pdf](docs/manual.pdf)
* 🔧 **คู่มือผังขาฮาร์ดแวร์และการต่อวงจร:** [hardware/pinout_diagram.md](hardware/pinout_diagram.md)
* ⚖️ **คู่มือขั้นตอนการสอบเทียบและตั้งศูนย์:** [hardware/calibration_guide.md](hardware/calibration_guide.md)
* 📊 **ตัวอย่างชุดข้อมูลวิจัย:** [data/sample_soil_ph_dataset.csv](data/sample_soil_ph_dataset.csv)

### บรรณานุกรมงานวิจัยอ้างอิง (Scientific References)
1. **Jenny, H., Nielsen, T. R., & Coleman, N. T. (1950).** Several remarks on the suspension effect in soil pH determination. *Science*, 112(2902), 164–167.
2. **Peech, M. (1965).** Hydrogen-ion activity. In *Methods of Soil Analysis: Part 2 Chemical and Microbiological Properties*, 9, 914–926.
3. **Nernst, W. (1889).** Die elektromotorische Wirksamkeit der Jonen. *Zeitschrift für physikalische Chemie*, 4(1), 129–181.
4. **Thomas, G. W. (1996).** Soil pH and soil acidity. *Methods of soil analysis: Part 3 Chemical methods*, 5, 475–490.
5. **USDA-NRCS. (2014).** *Soil Survey Field and Laboratory Methods Manual*. Soil Survey Investigations Report No. 51, Version 2.0.
6. **Viscarra Rossel, R. A., et al. (2010).** Proximal soil sensing: An effective approach for soil measurement in space and time. *Geoderma*, 158(1-2), 1–2.
7. **Adamchuk, V. I., et al. (2004).** On-the-go soil sensors for precision agriculture. *Computers and Electronics in Agriculture*, 44(1), 71–91.
8. **กรมพัฒนาที่ดิน. (2560).** *คู่มือการวิเคราะห์ตัวอย่างดิน น้ำ ปุ๋ย พืช และวัสดุปรับปรุงดิน*. สำนักวิทยาศาสตร์เพื่อการพัฒนาที่ดิน, กระทรวงเกษตรและสหกรณ์, กรุงเทพฯ.
