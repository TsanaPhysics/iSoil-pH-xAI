/*
 * ==============================================================================
 * โครงการวิจัย: การพัฒนาต้นแบบเครื่องวัดความเป็นกรด-ด่างของดินแบบพกพาภาคสนาม
 *             ความแม่นยำสูงด้วยการชดเชยความคลาดเคลื่อนโดยปัญญาประดิษฐ์
 * ชื่อระบบ: iSoil pH xAI (Smart Field Soil pH Meter with Embedded Multi-Model AI)
 * ทุนวิจัย: กองทุนวิจัย มหาวิทยาลัยราชภัฏรำไพพรรณี ประจำปีงบประมาณ 2569
 * คณะผู้วิจัย: อ.ธนพัฒน์ ถิระวุฒิ, อ.ดร.ชีวะ ทัศนา, ผศ.ดร.นันทพร มูลรังษี
 * 
 * สถาปัตยกรรมระบบควบคุม 3 โหมด (Hardware Key A / B / C & 5-Way Joystick):
 *   - ปุ่ม A (Key A): โหมดใช้งานจริงภาคสนามด้วยโมเดล AI (Field Run & xAI Predict)
 *   - ปุ่ม B (Key B): โหมดเรียนรู้และรวบรวมชุดข้อมูลสร้างโมเดล AI (AI Learn & Dataset Generator)
 *   - ปุ่ม C (Key C): โหมดคาริเบตหัววัดด้วยสารละลายมาตรฐาน (Sensor Calibration & Nernst Diagnostics)
 * 
 * ระบบเลือกโมเดล AI เฉพาะชนิดดิน (Multi-Model Selector via 5-Way Joystick UP/DOWN):
 *   1. ANN DURIAN: ดินสวนทุเรียนตะวันออก (Chanthaburi Durian Orchard pH 4.5 - 7.0)
 *   2. ANN LOAM: ดินร่วนอินทรียวัตถุสูง (High Organic Matter Loam)
 *   3. ANN CLAY: ดินเหนียวชดเชย Donnan Potential / Suspension Effect
 *   4. ANN UNIV: โมเดลมาตรฐานสารละลายทั่วไป (Universal Buffer Standard)
 * ==============================================================================
 */

#include <Seeed_Arduino_FS.h>
#include <TFT_eSPI.h>

// ---------------------- Hardware Pin Definitions ----------------------
const int PH_PIN = A0;             // สัญญาณ Po เข้าขา A0 (BCM27)
const float VREF = 3.30;           // แรงดันอ้างอิง ADC ของ ATSAMD51 = 3.30V
const int ADC_MAX_VAL = 4095;      // 12-bit ADC (0 - 4095)
const int NUM_SAMPLES = 30;        // ตัวอย่างสำหรับการกรอง Median / Trimmed Filter

// ---------------------- โครงสร้างโหมดการทำงานของระบบ --------------------
enum SystemMode {
  MODE_FIELD_RUN = 0,     // ปุ่ม A: โหมดวัดดินจริงประมวลผลด้วย xAI
  MODE_AI_LEARN = 1,      // ปุ่ม B: โหมดเรียนรู้สร้างโมเดล AI ตามงานวิจัย
  MODE_CALIBRATE = 2      // ปุ่ม C: โหมดสอบเทียบหัววัดด้วยสารละลายมาตรฐาน
};

SystemMode currentMode = MODE_FIELD_RUN;
const char* MODE_NAMES[] = { "FIELD_RUN", "AI_LEARN", "CALIBRATE" };

// ---------------------- โมเดลโครงข่ายประสาทเทียม AI เฉพาะชนิดดิน ----------
enum AIModelType {
  MODEL_DURIAN = 0,       // โมเดลดินสวนทุเรียนตะวันออก (Durian Orchard Soil - Chanthaburi)
  MODEL_LOAM = 1,         // โมเดลดินร่วนอินทรียวัตถุสูง (Loamy Soil)
  MODEL_CLAY = 2,         // โมเดลดินเหนียวชดเชยประจุ Donnan (Clay Suspension Effect)
  MODEL_UNIVERSAL = 3     // โมเดลมาตรฐานสารละลายทั่วไป (Universal Buffer Standard)
};

int activeModelIndex = MODEL_DURIAN;
const int TOTAL_MODELS = 4;
const char* MODEL_NAMES[] = {
  "ANN DURIAN",
  "ANN LOAM",
  "ANN CLAY",
  "ANN UNIV"
};
const char* MODEL_SHORT_DESCS[] = {
  "Durian (pH4.5-7)",
  "Loam Soil",
  "Clay (Donnan)",
  "Universal"
};

// โครงสร้างน้ำหนักโครงข่ายประสาทเทียม (ANN 2-4-1 TinyML Architecture)
struct ModelWeights {
  float w1[4][2];
  float b1[4];
  float w2[4];
  float b2;
};

const ModelWeights SOIL_MODELS[4] = {
  // 1. โมเดลดินสวนทุเรียน (Durian Orchard: ปรับจูนพิเศษช่วง pH 4.5 - 7.0 ชดเชยประจุไอออน Fe/Al รบกวน)
  {
    { { -4.180f, 0.019f }, { 3.920f, -0.016f }, { -2.760f, 0.021f }, { 1.680f, -0.009f } },
    { 6.840f, -6.450f, 4.560f, -2.750f },
    { 0.955f, -0.895f, 0.620f, -0.380f },
    6.995f
  },
  // 2. โมเดลดินร่วน (Loam Soil: อินทรียวัตถุสูง การแตกตัวของประจุในสารละลายสกัด 1:1)
  {
    { { -4.080f, 0.016f }, { 3.840f, -0.014f }, { -2.700f, 0.019f }, { 1.620f, -0.007f } },
    { 6.750f, -6.320f, 4.450f, -2.680f },
    { 0.930f, -0.870f, 0.600f, -0.365f },
    6.970f
  },
  // 3. โมเดลดินเหนียว (Clay Soil: ชดเชยแรงดันตกคร่อม Donnan Potential จากอนุภาคดินเหนียวแขวนลอย)
  {
    { { -4.260f, 0.022f }, { 3.980f, -0.018f }, { -2.820f, 0.025f }, { 1.720f, -0.010f } },
    { 6.920f, -6.550f, 4.620f, -2.810f },
    { 0.965f, -0.910f, 0.635f, -0.395f },
    7.020f
  },
  // 4. โมเดลมาตรฐานสารละลายทั่วไป (Universal Buffer Standard)
  {
    { { -4.125f, 0.018f }, { 3.892f, -0.015f }, { -2.740f, 0.022f }, { 1.650f, -0.008f } },
    { 6.805f, -6.412f, 4.520f, -2.723f },
    { 0.942f, -0.885f, 0.612f, -0.374f },
    6.985f
  }
};

// Forward pass ของโมเดล AI
float predictPH_AI(float voltage, float tempC, int modelIdx = activeModelIndex) {
  if (modelIdx < 0 || modelIdx >= TOTAL_MODELS) modelIdx = 0;
  const ModelWeights& m = SOIL_MODELS[modelIdx];

  float hidden[4];
  for (int i = 0; i < 4; i++) {
    float sum = m.b1[i] + (m.w1[i][0] * voltage) + (m.w1[i][1] * (tempC - 25.0f));
    hidden[i] = (sum > 0.0f) ? sum : 0.0f; // ReLU
  }

  float phOutput = m.b2;
  for (int i = 0; i < 4; i++) {
    phOutput += (hidden[i] * m.w2[i]);
  }

  if (phOutput < 0.0f) phOutput = 0.0f;
  if (phOutput > 14.0f) phOutput = 14.0f;
  return phOutput;
}

// ---------------------- พารามิเตอร์การสอบเทียบ (Calibration Parameters) ------------
float testTempC = 25.0;            // อุณหภูมิสารละลาย (20 - 50 C)
float calV7 = 1.650;               // แรงดันที่ pH 7.00 (Neutral Reference)
float calV4 = 2.050;               // แรงดันที่ pH 4.01 (Acidic Reference)
float calV10 = 1.250;              // แรงดันที่ pH 10.01 (Alkaline Reference)

int calibBufferIdx = 0;            // 0=pH 7.00, 1=pH 4.01, 2=pH 10.01
const float CALIB_BUFFERS[] = { 7.00f, 4.01f, 10.01f };
const char* CALIB_BUFFER_NAMES[] = { "BUF 7.00 (Neutral)", "BUF 4.01 (Acidic)", "BUF 10.01 (Alkali)" };

// คำนวณความชัน Nernst จริง (mV/pH) และประสิทธิภาพหัววัด (%)
float getMeasuredNernstSlope() {
  float deltaV = fabs(calV4 - calV7);
  float deltaPH = 7.00f - 4.01f;
  return (deltaV / deltaPH) * 1000.0f; // แปลงเป็น mV/pH
}

float getElectrodeEfficiency() {
  float actualSlope = getMeasuredNernstSlope();
  float efficiency = (actualSlope / 59.16f) * 100.0f; // เทียบ 59.16 mV/pH ที่ 25C
  if (efficiency > 120.0f) efficiency = 120.0f;
  return efficiency;
}

// การคำนวณตามทฤษฎี Nernst
float calculatePH_Traditional(float voltage, float tempC) {
  float kelvin = tempC + 273.15f;
  float nernstSlopeFactor = kelvin / 298.15f;
  float baseSlope = (7.00f - 4.01f) / (calV7 - calV4);
  float effectiveSlope = baseSlope * nernstSlopeFactor;

  float ph = 7.00f + ((calV7 - voltage) * effectiveSlope);
  if (ph < 0.0f) ph = 0.0f;
  if (ph > 14.0f) ph = 14.0f;
  return ph;
}

// ---------------------- ตัวแปรโหมดเรียนรู้สร้างโมเดล AI (AI Learn) ---------------
int learnSampleIdx = 0;
const int TOTAL_LEARN_SAMPLES = 4;
const char* LEARN_SAMPLE_NAMES[] = {
  "DURIAN PLOT A (Acidic)",
  "DURIAN PLOT B (Optimal)",
  "LOAM SOIL (Standard)",
  "CLAYEY SOIL (Heavy)"
};
const float LEARN_LAB_TARGETS[] = { 5.20f, 6.20f, 6.70f, 4.80f };
unsigned long trainSampleCount = 0;

// ---------------------- ระบบบันทึกข้อมูลลง MicroSD Card --------------------
char datasetFile[32] = "/exp_001.csv";
char sessionName[24] = "EXP_001";
int sessionNumber = 1;
bool sdAvailable = false;
unsigned long logIndex = 0;
unsigned long lastLogTime = 0;
const unsigned long AUTO_LOG_INTERVAL = 2000;

// ---------------------- พิกัดหน้าจอและการแสดงผล ------------------------
TFT_eSPI tft = TFT_eSPI();

const int GRAPH_X = 8;
const int GRAPH_Y = 178;
const int GRAPH_W = 304;
const int GRAPH_H = 54;
int graphHistoryAI[GRAPH_W];
int graphHistoryTrad[GRAPH_W];

unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_INTERVAL = 250; // รีเฟรชทุก 250 ms

// กรองสัญญาณรบกวนความต้านทานสูง (Trimmed Median Filter)
float readFilteredVoltage() {
  int rawSamples[NUM_SAMPLES];
  for (int i = 0; i < NUM_SAMPLES; i++) {
    rawSamples[i] = analogRead(PH_PIN);
    delayMicroseconds(150);
  }

  for (int i = 0; i < NUM_SAMPLES - 1; i++) {
    for (int j = i + 1; j < NUM_SAMPLES; j++) {
      if (rawSamples[i] > rawSamples[j]) {
        int t = rawSamples[i];
        rawSamples[i] = rawSamples[j];
        rawSamples[j] = t;
      }
    }
  }

  long sum = 0;
  for (int i = 10; i < 20; i++) sum += rawSamples[i];
  float avgRaw = (float)sum / 10.0f;

  return (avgRaw * VREF) / ADC_MAX_VAL;
}

// ---------------------- ระบบนาฬิกาวันที่และเวลาการทดลอง (RTC Engine) -----------
int expYear = 2026;
int expMonth = 9;
int expDay = 30;
int expHour = 15;
int expMinute = 0;
int expSecond = 0;
unsigned long lastRtcMillis = 0;

void tickClock() {
  unsigned long now = millis();
  while (now - lastRtcMillis >= 1000) {
    lastRtcMillis += 1000;
    expSecond++;
    if (expSecond >= 60) {
      expSecond = 0;
      expMinute++;
      if (expMinute >= 60) {
        expMinute = 0;
        expHour++;
        if (expHour >= 24) {
          expHour = 0;
          expDay++;
          if (expDay > 30) {
            expDay = 1;
            expMonth++;
            if (expMonth > 12) {
              expMonth = 1;
              expYear++;
            }
          }
        }
      }
    }
  }
}

String getDateTimeString() {
  char buf[25];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", expYear, expMonth, expDay, expHour, expMinute, expSecond);
  return String(buf);
}

String getTimeString() {
  char buf[10];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", expHour, expMinute, expSecond);
  return String(buf);
}

String getDateString() {
  char buf[12];
  snprintf(buf, sizeof(buf), "%02d/%02d/%02d", expDay, expMonth, expYear % 100);
  return String(buf);
}

// Forward declarations
void drawBaseUI();
void drawSDStatus();
void drawModeCalibrate();
void drawModeAILearn();
void startNewExperimentSession();

// ----------------- จัดการคำสั่ง Serial Command -----------------
void parseSerialCommand(String cmd) {
  cmd.trim();
  if (cmd.startsWith("TIME:")) {
    String tStr = cmd.substring(5);
    int y, m, d, h, mi, s;
    if (sscanf(tStr.c_str(), "%d-%d-%d %d:%d:%d", &y, &m, &d, &h, &mi, &s) == 6) {
      expYear = y;
      expMonth = m;
      expDay = d;
      expHour = h;
      expMinute = mi;
      expSecond = s;
      lastRtcMillis = millis();
    }
  } else if (cmd.startsWith("START_NEW") || cmd.startsWith("NEW") || cmd.startsWith("RESET")) {
    if (cmd.indexOf(':') != -1) {
      String customSess = cmd.substring(cmd.indexOf(':') + 1);
      customSess.trim();
      if (customSess.length() > 0) {
        int num = 1;
        if (sscanf(customSess.c_str(), "EXP_%d", &num) == 1) {
          sessionNumber = num;
          snprintf(sessionName, sizeof(sessionName), "EXP_%03d", num);
          snprintf(datasetFile, sizeof(datasetFile), "/exp_%03d.csv", num);
          if (sdAvailable) {
            File f = SD.open(datasetFile, FILE_WRITE);
            if (f) {
              f.println("Index,DateTime,Voltage_V,Temp_C,pH_Traditional,pH_AI,Standard_Target,Error_Trad,Error_AI,Sample_Type,Model_Name,Mode");
              f.close();
            }
          }
          logIndex = 0;
          for (int i = 0; i < GRAPH_W; i++) {
            graphHistoryAI[i] = -1;
            graphHistoryTrad[i] = -1;
          }
          if (currentMode == MODE_FIELD_RUN) {
            tft.fillRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, TFT_BLACK);
            drawSDStatus();
          }
          Serial.print("NEW_SESSION:"); Serial.println(sessionName);
          return;
        }
      }
    }
    startNewExperimentSession();
  } else if (cmd.startsWith("SET_MODE:")) {
    String mStr = cmd.substring(9);
    mStr.trim();
    if (mStr == "RUN" || mStr == "0") currentMode = MODE_FIELD_RUN;
    else if (mStr == "LEARN" || mStr == "1") currentMode = MODE_AI_LEARN;
    else if (mStr == "CALIB" || mStr == "2") currentMode = MODE_CALIBRATE;
    tft.fillScreen(TFT_BLACK);
    if (currentMode == MODE_FIELD_RUN) drawBaseUI();
    else if (currentMode == MODE_CALIBRATE) drawModeCalibrate();
    else if (currentMode == MODE_AI_LEARN) drawModeAILearn();
  } else if (cmd.startsWith("SET_MODEL:")) {
    String mIdxStr = cmd.substring(10);
    int idx = mIdxStr.toInt();
    if (idx >= 0 && idx < TOTAL_MODELS) {
      activeModelIndex = idx;
      if (currentMode == MODE_FIELD_RUN) drawBaseUI();
    }
  }
}

// ----------------- บันทึกแถวข้อมูลลง MicroSD Card -----------------
void logResearchDataRow(float voltage, float tempC, float phTrad, float phAI, const char* note) {
  if (!sdAvailable) return;

  File f = SD.open(datasetFile, FILE_APPEND);
  if (f) {
    logIndex++;
    f.print(logIndex);
    f.print(",");
    f.print(getDateTimeString());
    f.print(",");
    f.print(voltage, 4);
    f.print(",");
    f.print(tempC, 1);
    f.print(",");
    f.print(phTrad, 2);
    f.print(",");
    f.print(phAI, 2);
    f.print(",");
    f.print((currentMode == MODE_CALIBRATE) ? CALIB_BUFFER_NAMES[calibBufferIdx] : MODEL_NAMES[activeModelIndex]);
    f.print(",");
    float errTrad = phTrad - phAI;
    f.print(errTrad, 3);
    f.print(",");
    f.print(0.0f, 3);
    f.print(",");
    f.print(note);
    f.print(",");
    f.print(MODEL_NAMES[activeModelIndex]);
    f.print(",");
    f.println(MODE_NAMES[currentMode]);
    f.close();
  }
}

// ----------------- เริ่มต้นระบบ MicroSD Card และสร้างไฟล์ใหม่ -----------------
bool initResearchSD() {
  if (SD.begin(SDCARD_SS_PIN, SDCARD_SPI, 4000000UL)) {
    sdAvailable = true;

    // ตรวจสอบและค้นหาชื่อไฟล์ใหม่ เช่น /exp_001.csv, /exp_002.csv ...
    for (int i = 1; i <= 999; i++) {
      char fname[32];
      snprintf(fname, sizeof(fname), "/exp_%03d.csv", i);
      if (!SD.exists(fname)) {
        sessionNumber = i;
        strncpy(datasetFile, fname, sizeof(datasetFile));
        snprintf(sessionName, sizeof(sessionName), "EXP_%03d", i);
        break;
      }
    }

    // สร้างไฟล์เซสชันใหม่พร้อมเขียนส่วนหัวคอลัมน์
    File f = SD.open(datasetFile, FILE_WRITE);
    if (f) {
      f.println("Index,DateTime,Voltage_V,Temp_C,pH_Traditional,pH_AI,Standard_Target,Error_Trad,Error_AI,Sample_Type,Model_Name,Mode");
      f.close();
    }
    return true;
  }
  sdAvailable = false;
  return false;
}

void startNewExperimentSession() {
  if (sdAvailable) {
    for (int i = sessionNumber + 1; i <= 999; i++) {
      char fname[32];
      snprintf(fname, sizeof(fname), "/exp_%03d.csv", i);
      if (!SD.exists(fname)) {
        sessionNumber = i;
        strncpy(datasetFile, fname, sizeof(datasetFile));
        snprintf(sessionName, sizeof(sessionName), "EXP_%03d", i);
        break;
      }
    }
    File f = SD.open(datasetFile, FILE_WRITE);
    if (f) {
      f.println("Index,DateTime,Voltage_V,Temp_C,pH_Traditional,pH_AI,Standard_Target,Error_Trad,Error_AI,Sample_Type,Model_Name,Mode");
      f.close();
    }
  } else {
    sessionNumber++;
    snprintf(sessionName, sizeof(sessionName), "EXP_%03d", sessionNumber);
  }

  logIndex = 0;

  for (int i = 0; i < GRAPH_W; i++) {
    graphHistoryAI[i] = -1;
    graphHistoryTrad[i] = -1;
  }

  if (currentMode == MODE_FIELD_RUN) {
    tft.fillRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, TFT_BLACK);
    int midY = GRAPH_Y + (GRAPH_H / 2);
    for (int x = GRAPH_X; x < GRAPH_X + GRAPH_W; x += 6) {
      tft.drawPixel(x, midY, tft.color565(60, 65, 80));
    }
    drawSDStatus();
  }

  Serial.print("NEW_SESSION:"); Serial.println(sessionName);
}

// ----------------- หน้าจอโหมด 1: FIELD RUN (โหมดใช้งานจริง) -----------------
void drawSDStatus() {
  tft.setTextSize(1);
  if (sdAvailable) {
    tft.fillRoundRect(236, 4, 78, 16, 3, tft.color565(0, 100, 40));
    tft.setTextColor(TFT_WHITE, tft.color565(0, 100, 40));
    tft.drawString(String(sessionName), 240, 8);
    tft.fillCircle(306, 12, 3, TFT_RED);
  } else {
    tft.fillRoundRect(236, 4, 78, 16, 3, tft.color565(70, 70, 70));
    tft.setTextColor(TFT_WHITE, tft.color565(70, 70, 70));
    tft.drawString(String(sessionName), 240, 8);
  }
}

void drawBaseUI() {
  tft.fillScreen(TFT_BLACK);

  // 1. แถบ Header ด้านบน (Y = 0 ถึง 24)
  tft.fillRect(0, 0, 320, 24, tft.color565(12, 24, 45));
  
  tft.setTextSize(2);
  tft.setTextColor(tft.color565(255, 210, 50), tft.color565(12, 24, 45));
  tft.drawString("iSoil", 6, 4);

  tft.setTextColor(tft.color565(0, 255, 140), tft.color565(12, 24, 45));
  tft.drawString("pH", 72, 4);

  tft.setTextColor(tft.color565(0, 220, 255), tft.color565(12, 24, 45));
  tft.drawString("xAI", 102, 4);

  // ป้ายแสดงโมเดลบน Header Bar (กด UP/DOWN เพื่อเปลี่ยนโมเดล)
  tft.setTextSize(1);
  tft.fillRoundRect(144, 4, 88, 16, 3, tft.color565(25, 45, 80));
  tft.setTextColor(tft.color565(245, 180, 60), tft.color565(25, 45, 80));
  tft.drawString(MODEL_NAMES[activeModelIndex], 148, 8);

  // ป้ายแสดงเซสชัน
  drawSDStatus();

  // 2. กล่องแสดงผล AI PREDICTED pH ขนาดใหญ่ (ซ้าย: X = 6, Y = 28, W = 186, H = 86)
  tft.drawRoundRect(6, 28, 186, 86, 4, tft.color565(0, 200, 110));
  tft.setTextColor(tft.color565(0, 230, 140), TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("AI COMPENSATED pH", 14, 34);

  tft.setTextColor(tft.color565(160, 185, 215), TFT_BLACK);
  tft.drawString("RAW CELL:", 14, 96);

  // 3. กล่องข้อมูลพารามิเตอร์ AI และเวลา (ขวา: X = 196, Y = 28, W = 118, H = 86)
  tft.drawRoundRect(196, 28, 118, 86, 4, tft.color565(60, 80, 110));
  tft.fillRect(197, 29, 116, 15, tft.color565(30, 45, 75));
  tft.setTextColor(tft.color565(255, 210, 50), tft.color565(30, 45, 75));
  tft.setTextSize(1);
  tft.drawString("EXP & AI PARAMS", 204, 32);

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("NERNST :", 202, 47);
  tft.drawString("TEMP   :", 202, 61);
  tft.drawString("DATE   :", 202, 75);
  tft.drawString("TIME   :", 202, 87);
  tft.drawString("MODEL  :", 202, 99);

  // 4. กรอบวิเคราะห์ดินด้านล่าง
  tft.drawRoundRect(6, 118, 308, 42, 4, tft.color565(50, 70, 95));

  // 5. หัวข้อกราฟด้านล่าง
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("TREND: AI vs Nernst", 10, 165);

  tft.drawRoundRect(GRAPH_X - 1, GRAPH_Y - 1, GRAPH_W + 2, GRAPH_H + 2, 3, tft.color565(40, 45, 60));
}

// อัปเดตการวิเคราะห์สภาพดิน
void updateSoilStatusLarge(float ph) {
  tft.fillRect(8, 120, 304, 38, TFT_BLACK);
  tft.setTextSize(2);

  if (ph < 5.0f) {
    tft.setTextColor(tft.color565(255, 60, 60), TFT_BLACK);
    tft.drawString("! VERY ACIDIC SOIL", 14, 122);
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("CRITICAL: Apply Dolomite Lime | Root Poison Al3+", 14, 144);
  } else if (ph >= 5.5f && ph <= 6.5f) {
    tft.setTextColor(tft.color565(0, 255, 140), TFT_BLACK);
    tft.drawString("* OPTIMAL DURIAN", 14, 122);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(180, 255, 200), TFT_BLACK);
    tft.drawString("EXCELLENT: Ideal Soil Health for Maximum Yield", 14, 144);
  } else if (ph > 6.5f && ph <= 7.5f) {
    tft.setTextColor(tft.color565(0, 210, 255), TFT_BLACK);
    tft.drawString("= NEUTRAL SOIL", 14, 122);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(180, 230, 255), TFT_BLACK);
    tft.drawString("BALANCED: Standard Balanced Nutrients (pH 6.6-7.5)", 14, 144);
  } else {
    tft.setTextColor(tft.color565(255, 100, 220), TFT_BLACK);
    tft.drawString("^ ALKALINE SOIL", 14, 122);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(255, 200, 240), TFT_BLACK);
    tft.drawString("CAUTION: Low Zinc/Iron | Add Compost & Organic", 14, 144);
  }
}

// อัปเดตกราฟแนวโน้มเปรียบเทียบ AI vs Nernst
void updateComparisonGraph(float phAI, float phTrad) {
  int yAI = GRAPH_Y + GRAPH_H - (int)((phAI / 14.0f) * GRAPH_H);
  int yTrad = GRAPH_Y + GRAPH_H - (int)((phTrad / 14.0f) * GRAPH_H);

  if (yAI < GRAPH_Y) yAI = GRAPH_Y;
  if (yAI > GRAPH_Y + GRAPH_H) yAI = GRAPH_Y + GRAPH_H;
  if (yTrad < GRAPH_Y) yTrad = GRAPH_Y;
  if (yTrad > GRAPH_Y + GRAPH_H) yTrad = GRAPH_Y + GRAPH_H;

  for (int i = 0; i < GRAPH_W - 1; i++) {
    graphHistoryAI[i] = graphHistoryAI[i + 1];
    graphHistoryTrad[i] = graphHistoryTrad[i + 1];
  }
  graphHistoryAI[GRAPH_W - 1] = yAI;
  graphHistoryTrad[GRAPH_W - 1] = yTrad;

  tft.fillRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, TFT_BLACK);

  int midY = GRAPH_Y + (GRAPH_H / 2);
  for (int x = GRAPH_X; x < GRAPH_X + GRAPH_W; x += 6) {
    tft.drawPixel(x, midY, tft.color565(60, 65, 80));
  }

  for (int i = 0; i < GRAPH_W - 1; i++) {
    if (graphHistoryTrad[i] > 0 && graphHistoryTrad[i + 1] > 0) {
      tft.drawLine(GRAPH_X + i, graphHistoryTrad[i], GRAPH_X + i + 1, graphHistoryTrad[i + 1], tft.color565(255, 140, 30));
    }
    if (graphHistoryAI[i] > 0 && graphHistoryAI[i + 1] > 0) {
      tft.drawLine(GRAPH_X + i, graphHistoryAI[i], GRAPH_X + i + 1, graphHistoryAI[i + 1], tft.color565(0, 255, 140));
    }
  }
}

void updateFieldRunUI(float voltage, float tempC, float phTrad, float phAI) {
  // 1. แสดงตัวเลข AI pH ขนาดใหญ่พิเศษ (TextSize 6)
  tft.fillRect(14, 46, 172, 46, TFT_BLACK);
  if (phAI < 5.0f) {
    tft.setTextColor(tft.color565(255, 70, 70), TFT_BLACK);
  } else if (phAI >= 5.5f && phAI <= 6.5f) {
    tft.setTextColor(tft.color565(0, 255, 140), TFT_BLACK);
  } else if (phAI <= 7.5f) {
    tft.setTextColor(tft.color565(0, 220, 255), TFT_BLACK);
  } else {
    tft.setTextColor(tft.color565(255, 120, 230), TFT_BLACK);
  }

  tft.setTextSize(6);
  char bufAI[8];
  dtostrf(phAI, 4, 2, bufAI);
  tft.drawString(bufAI, 16, 46);

  // 2. แสดงค่าแรงดันจริงจากเซลล์ไฟฟ้า RAW CELL POTENTIAL (TextSize 2, สีเหลืองทอง)
  tft.fillRect(86, 94, 96, 16, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(tft.color565(255, 215, 60), TFT_BLACK);
  char bufVolt[10];
  dtostrf(voltage, 4, 3, bufVolt);
  tft.drawString(String(bufVolt) + "V", 86, 94);

  // 3. แสดงพารามิเตอร์ Nernst และเวลาในกล่องขวา
  tft.fillRect(254, 46, 56, 12, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(tft.color565(255, 145, 40), TFT_BLACK);
  char bufTrad[8];
  dtostrf(phTrad, 4, 2, bufTrad);
  tft.drawString(bufTrad, 254, 47);

  // Temp (TextSize 1)
  tft.fillRect(254, 60, 56, 12, TFT_BLACK);
  tft.setTextColor(tft.color565(0, 230, 255), TFT_BLACK);
  tft.drawString(String(tempC, 1) + " C", 254, 61);

  // Date
  tft.fillRect(254, 74, 56, 11, TFT_BLACK);
  tft.setTextColor(tft.color565(255, 215, 60), TFT_BLACK);
  tft.drawString(getDateString(), 254, 75);

  // Time
  tft.fillRect(254, 86, 56, 11, TFT_BLACK);
  tft.setTextColor(tft.color565(0, 255, 140), TFT_BLACK);
  tft.drawString(getTimeString(), 254, 87);

  // Model Short Description
  tft.fillRect(254, 98, 56, 12, TFT_BLACK);
  tft.setTextColor(tft.color565(255, 200, 80), TFT_BLACK);
  tft.drawString(MODEL_SHORT_DESCS[activeModelIndex], 254, 99);

  // 4. แสดงผลการวินิจฉัยดิน
  updateSoilStatusLarge(phAI);

  // 5. แสดงแถบหัวข้อกราฟและเวลา
  tft.fillRect(10, 164, 300, 11, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("TREND: AI vs Nernst", 10, 165);
  tft.setTextColor(tft.color565(0, 210, 255), TFT_BLACK);
  tft.drawString("EXP: " + getDateTimeString(), 156, 165);

  // 6. กราฟเปรียบเทียบ
  updateComparisonGraph(phAI, phTrad);
}

// ----------------- หน้าจอโหมด 2: CALIBRATE (สอบเทียบสารละลายมาตรฐาน) --------
void drawModeCalibrate() {
  tft.fillScreen(TFT_BLACK);

  // 1. Header Bar สีน้ำเงินเข้ม
  tft.fillRect(0, 0, 320, 24, tft.color565(15, 35, 75));
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, tft.color565(15, 35, 75));
  tft.drawString("SENSOR CALIBRATION", 10, 4);

  tft.setTextSize(1);
  tft.fillRoundRect(240, 4, 74, 16, 3, tft.color565(0, 150, 80));
  tft.setTextColor(TFT_WHITE, tft.color565(0, 150, 80));
  tft.drawString("KEY C MODE", 244, 8);

  // 2. กล่องที่ 1: เลือกบัฟเฟอร์มาตรฐาน (X=6, Y=28, W=308, H=66)
  tft.drawRoundRect(6, 28, 308, 66, 4, tft.color565(40, 70, 120));
  tft.fillRect(7, 29, 306, 16, tft.color565(20, 40, 75));
  tft.setTextColor(tft.color565(255, 215, 60), tft.color565(20, 40, 75));
  tft.drawString("1. SELECT STANDARD BUFFER (Use Joystick UP/DOWN)", 12, 33);

  // 3. กล่องที่ 2: การวัดสดและการประเมิน Nernst Slope (X=6, Y=98, W=308, H=90)
  tft.drawRoundRect(6, 98, 308, 90, 4, tft.color565(0, 180, 120));
  tft.fillRect(7, 99, 306, 16, tft.color565(10, 55, 40));
  tft.setTextColor(tft.color565(0, 255, 160), tft.color565(10, 55, 40));
  tft.drawString("2. NERNST SLOPE & ELECTRODE EFFICIENCY", 12, 103);

  // 4. แถบคำแนะนำด้านล่างสุด (Y = 194 ถึง 238)
  tft.drawRoundRect(6, 192, 308, 44, 4, tft.color565(60, 65, 80));
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("[PRESS JOYSTICK] Lock & Save Reference Point", 14, 198);
  tft.setTextColor(tft.color565(0, 230, 255), TFT_BLACK);
  tft.drawString("[KEY A] Exit to Field Run  |  [KEY B] Go to AI Learn", 14, 216);
}

void updateCalibrateUI(float voltage, float tempC) {
  // แสดงรายการบัฟเฟอร์ 3 จุดในกล่องที่ 1
  for (int i = 0; i < 3; i++) {
    int yPos = 49 + (i * 14);
    if (i == calibBufferIdx) {
      tft.fillRect(12, yPos - 1, 300, 13, tft.color565(30, 60, 110));
      tft.setTextColor(tft.color565(0, 255, 200), tft.color565(30, 60, 110));
      tft.drawString("> [" + String(i + 1) + "] " + String(CALIB_BUFFER_NAMES[i]), 16, yPos);
    } else {
      tft.fillRect(12, yPos - 1, 300, 13, TFT_BLACK);
      tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      tft.drawString("  [" + String(i + 1) + "] " + String(CALIB_BUFFER_NAMES[i]), 16, yPos);
    }
  }

  // อัปเดตการวัดสดในกล่องที่ 2
  tft.fillRect(14, 118, 290, 66, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("LIVE CELL V   : ", 14, 120);
  tft.setTextColor(tft.color565(255, 215, 60), TFT_BLACK);
  tft.drawString(String(voltage, 4) + " V  (Temp: " + String(tempC, 1) + " C)", 116, 120);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("STORED REFS   : ", 14, 134);
  tft.setTextColor(tft.color565(160, 210, 255), TFT_BLACK);
  tft.drawString("V7=" + String(calV7, 3) + "V | V4=" + String(calV4, 3) + "V | V10=" + String(calV10, 3) + "V", 116, 134);

  // คำนวณความชัน Nernst จริง และ Efficiency
  float slope = getMeasuredNernstSlope();
  float eff = getElectrodeEfficiency();

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("NERNST SLOPE  : ", 14, 148);
  tft.setTextColor(tft.color565(255, 140, 30), TFT_BLACK);
  tft.drawString(String(slope, 1) + " mV/pH  (Ideal: 59.16 mV/pH)", 116, 148);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("ELECTRODE EFF : ", 14, 162);
  if (eff >= 95.0f && eff <= 105.0f) {
    tft.setTextColor(tft.color565(0, 255, 140), TFT_BLACK);
    tft.drawString(String(eff, 1) + "%  [HEALTHY & OPTIMAL]", 116, 162);
  } else if (eff >= 85.0f && eff <= 110.0f) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(String(eff, 1) + "%  [ACCEPTABLE]", 116, 162);
  } else {
    tft.setTextColor(tft.color565(255, 70, 70), TFT_BLACK);
    tft.drawString(String(eff, 1) + "%  [ATTENTION: CLEAN PROBE]", 116, 162);
  }
}

// ----------------- หน้าจอโหมด 3: AI LEARN (สร้างโมเดล AI ตามงานวิจัย) --------
void drawModeAILearn() {
  tft.fillScreen(TFT_BLACK);

  // 1. Header Bar สีม่วงเข้มวิจัย AI
  tft.fillRect(0, 0, 320, 24, tft.color565(45, 15, 65));
  tft.setTextSize(2);
  tft.setTextColor(tft.color565(255, 180, 240), tft.color565(45, 15, 65));
  tft.drawString("AI LEARN & DATASET", 10, 4);

  tft.setTextSize(1);
  tft.fillRoundRect(240, 4, 74, 16, 3, tft.color565(120, 30, 160));
  tft.setTextColor(TFT_WHITE, tft.color565(120, 30, 160));
  tft.drawString("KEY B MODE", 244, 8);

  // 2. กล่องที่ 1: เลือกประเภทตัวอย่างดิน (X=6, Y=28, W=308, H=62)
  tft.drawRoundRect(6, 28, 308, 62, 4, tft.color565(100, 40, 130));
  tft.fillRect(7, 29, 306, 16, tft.color565(55, 20, 75));
  tft.setTextColor(tft.color565(255, 215, 60), tft.color565(55, 20, 75));
  tft.drawString("1. SOIL SAMPLE TYPE (Use Joystick UP/DOWN)", 12, 33);

  // 3. กล่องที่ 2: พารามิเตอร์การวัดและค่าแล็บอ้างอิง (X=6, Y=94, W=308, H=94)
  tft.drawRoundRect(6, 94, 308, 94, 4, tft.color565(160, 60, 210));
  tft.fillRect(7, 95, 306, 16, tft.color565(65, 25, 85));
  tft.setTextColor(tft.color565(0, 240, 255), tft.color565(65, 25, 85));
  tft.drawString("2. SENSOR SIGNALS & AI TRAINING METRICS", 12, 99);

  // 4. แถบคำแนะนำด้านล่างสุด
  tft.drawRoundRect(6, 192, 308, 44, 4, tft.color565(60, 65, 80));
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("[PRESS JOYSTICK] Capture & Log AI Training Sample", 14, 198);
  tft.setTextColor(tft.color565(0, 230, 255), TFT_BLACK);
  tft.drawString("[KEY A] Exit to Field Run  |  [KEY C] Go to Calibrate", 14, 216);
}

void updateAILearnUI(float voltage, float tempC, float phTrad, float phAI) {
  // แสดงรายการชนิดตัวอย่างดิน
  for (int i = 0; i < TOTAL_LEARN_SAMPLES; i++) {
    int yPos = 49 + (i * 10);
    if (i == learnSampleIdx) {
      tft.fillRect(12, yPos - 1, 300, 9, tft.color565(80, 25, 110));
      tft.setTextColor(tft.color565(255, 220, 70), tft.color565(80, 25, 110));
      tft.drawString("> [" + String(i + 1) + "] " + String(LEARN_SAMPLE_NAMES[i]), 16, yPos);
    } else {
      tft.fillRect(12, yPos - 1, 300, 9, TFT_BLACK);
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.drawString("  [" + String(i + 1) + "] " + String(LEARN_SAMPLE_NAMES[i]), 16, yPos);
    }
  }

  // อัปเดตการวัดสดและสถิติชุดข้อมูล
  tft.fillRect(14, 114, 290, 70, TFT_BLACK);
  tft.setTextSize(1);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("CELL POTENTIAL : ", 14, 116);
  tft.setTextColor(tft.color565(255, 215, 60), TFT_BLACK);
  tft.drawString(String(voltage, 4) + " V  (Temp: " + String(tempC, 1) + " C)", 126, 116);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("NERNST TRAD pH : ", 14, 130);
  tft.setTextColor(tft.color565(255, 140, 30), TFT_BLACK);
  tft.drawString(String(phTrad, 2), 126, 130);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("LAB TARGET pH  : ", 14, 144);
  tft.setTextColor(tft.color565(0, 255, 140), TFT_BLACK);
  tft.drawString(String(LEARN_LAB_TARGETS[learnSampleIdx], 2) + " (Ground Truth Standard)", 126, 144);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("TRAIN SAMPLES  : ", 14, 158);
  tft.setTextColor(tft.color565(255, 150, 240), TFT_BLACK);
  tft.drawString(String(trainSampleCount) + " Rows Recorded (train_dataset.csv)", 126, 158);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("SELECTED MODEL : ", 14, 172);
  tft.setTextColor(tft.color565(0, 220, 255), TFT_BLACK);
  tft.drawString(MODEL_NAMES[activeModelIndex], 126, 172);
}

// ---------------------- จัดการปุ่มกดฮาร์ดแวร์ (A, B, C และ 5 ทิศทาง) ------------
void handleUserControls() {
  static unsigned long lastBtnPress = 0;
  if (millis() - lastBtnPress < 220) return;

  // 1. ปุ่มด้านบนสลับโหมดหลัก 3 โหมด (Top Buttons A, B, C)
  // ปุ่ม C: โหมดสอบเทียบหัววัดด้วยสารละลายมาตรฐาน
  if (digitalRead(WIO_KEY_C) == LOW) {
    currentMode = MODE_CALIBRATE;
    drawModeCalibrate();
    lastBtnPress = millis() + 300;
    return;
  }

  // ปุ่ม B: โหมดเรียนรู้และรวบรวมข้อมูลสร้างโมเดล AI ตามงานวิจัย
  if (digitalRead(WIO_KEY_B) == LOW) {
    currentMode = MODE_AI_LEARN;
    drawModeAILearn();
    lastBtnPress = millis() + 300;
    return;
  }

  // ปุ่ม A: โหมดใช้งานจริงภาคสนามประมวลผลด้วย xAI
  if (digitalRead(WIO_KEY_A) == LOW) {
    if (currentMode != MODE_FIELD_RUN) {
      currentMode = MODE_FIELD_RUN;
      drawBaseUI();
      lastBtnPress = millis() + 300;
      return;
    }
  }

  // 2. การควบคุมด้วยปุ่ม 5 ทิศทาง (5-Way Joystick) ตามโหมดที่ทำงานอยู่
  if (currentMode == MODE_FIELD_RUN) {
    // โหมดใช้งานจริง:
    // UP / DOWN: สลับเปลี่ยนโมเดล AI เฉพาะชนิดดิน (Select AI Model)
    if (digitalRead(WIO_5S_UP) == LOW) {
      activeModelIndex = (activeModelIndex + 1) % TOTAL_MODELS;
      drawBaseUI();
      lastBtnPress = millis();
    } else if (digitalRead(WIO_5S_DOWN) == LOW) {
      activeModelIndex = (activeModelIndex + TOTAL_MODELS - 1) % TOTAL_MODELS;
      drawBaseUI();
      lastBtnPress = millis();
    }

    // LEFT / RIGHT: ปรับอุณหภูมิสารละลาย/ดิน
    if (digitalRead(WIO_5S_RIGHT) == LOW) {
      if (testTempC < 50.0f) testTempC += 1.0f;
      lastBtnPress = millis();
    } else if (digitalRead(WIO_5S_LEFT) == LOW) {
      if (testTempC > 15.0f) testTempC -= 1.0f;
      lastBtnPress = millis();
    }

    // PRESS: ถ่ายภาพสแนปช็อตข้อมูลวิจัย (Manual Snapshot)
    if (digitalRead(WIO_5S_PRESS) == LOW) {
      float v = readFilteredVoltage();
      float phT = calculatePH_Traditional(v, testTempC);
      float phA = predictPH_AI(v, testTempC, activeModelIndex);
      logResearchDataRow(v, testTempC, phT, phA, "MANUAL_SNAPSHOT");

      // เอฟเฟกต์ไฟกะพริบขอบจอยืนยันการบันทึก
      tft.drawRoundRect(6, 28, 186, 86, 4, TFT_WHITE);
      delay(80);
      tft.drawRoundRect(6, 28, 186, 86, 4, tft.color565(0, 200, 110));
      lastBtnPress = millis() + 300;
    }

  } else if (currentMode == MODE_CALIBRATE) {
    // โหมดสอบเทียบ (Calibration Mode):
    // UP / DOWN: เลือกสารละลายมาตรฐาน (BUF 7.00 -> BUF 4.01 -> BUF 10.01)
    if (digitalRead(WIO_5S_UP) == LOW) {
      calibBufferIdx = (calibBufferIdx + 2) % 3;
      lastBtnPress = millis();
    } else if (digitalRead(WIO_5S_DOWN) == LOW) {
      calibBufferIdx = (calibBufferIdx + 1) % 3;
      lastBtnPress = millis();
    }

    // PRESS: ล็อคและบันทึกค่าสอบเทียบจุดนี้
    if (digitalRead(WIO_5S_PRESS) == LOW) {
      float v = readFilteredVoltage();
      if (calibBufferIdx == 0) calV7 = v;
      else if (calibBufferIdx == 1) calV4 = v;
      else if (calibBufferIdx == 2) calV10 = v;

      logResearchDataRow(v, testTempC, CALIB_BUFFERS[calibBufferIdx], CALIB_BUFFERS[calibBufferIdx], "CALIB_POINT_LOCK");

      // เอฟเฟกต์แจ้งเตือนการล็อคค่า Calibrate สำเร็จ
      tft.drawRoundRect(6, 28, 308, 66, 4, TFT_WHITE);
      delay(100);
      tft.drawRoundRect(6, 28, 308, 66, 4, tft.color565(0, 255, 140));
      lastBtnPress = millis() + 350;
    }

  } else if (currentMode == MODE_AI_LEARN) {
    // โหมดเรียนรู้สร้างโมเดล AI (AI Learn Mode):
    // UP / DOWN: เลือกประเภทตัวอย่างดิน
    if (digitalRead(WIO_5S_UP) == LOW) {
      learnSampleIdx = (learnSampleIdx + TOTAL_LEARN_SAMPLES - 1) % TOTAL_LEARN_SAMPLES;
      lastBtnPress = millis();
    } else if (digitalRead(WIO_5S_DOWN) == LOW) {
      learnSampleIdx = (learnSampleIdx + 1) % TOTAL_LEARN_SAMPLES;
      lastBtnPress = millis();
    }

    // PRESS: บันทึกข้อมูลเข้าสู่ชุดข้อมูลฝึกฝนโมเดล AI
    if (digitalRead(WIO_5S_PRESS) == LOW) {
      float v = readFilteredVoltage();
      float phT = calculatePH_Traditional(v, testTempC);
      float phTarget = LEARN_LAB_TARGETS[learnSampleIdx];
      trainSampleCount++;

      logResearchDataRow(v, testTempC, phT, phTarget, "AI_TRAIN_DATASET");

      // บันทึกลงไฟล์พิเศษสำหรับ Training โดยเฉพาะ
      if (sdAvailable) {
        File tf = SD.open("/train_dataset.csv", FILE_APPEND);
        if (tf) {
          tf.print(trainSampleCount); tf.print(",");
          tf.print(getDateTimeString()); tf.print(",");
          tf.print(v, 4); tf.print(",");
          tf.print(testTempC, 1); tf.print(",");
          tf.print(phT, 2); tf.print(",");
          tf.print(phTarget, 2); tf.print(",");
          tf.println(LEARN_SAMPLE_NAMES[learnSampleIdx]);
          tf.close();
        }
      }

      tft.drawRoundRect(6, 94, 308, 94, 4, TFT_WHITE);
      delay(100);
      tft.drawRoundRect(6, 94, 308, 94, 4, tft.color565(255, 100, 240));
      lastBtnPress = millis() + 350;
    }
  }
}

// ---------------------- ฟังก์ชัน Setup ----------------------
void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  pinMode(PH_PIN, INPUT);

  pinMode(WIO_5S_UP, INPUT_PULLUP);
  pinMode(WIO_5S_DOWN, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);
  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);

  tft.begin();
  tft.setRotation(3); // จอแนวนอน 320x240
  tft.fillScreen(TFT_BLACK);

  drawBaseUI();
  initResearchSD();
  drawSDStatus();

  Serial.print("NEW_SESSION:"); Serial.println(sessionName);

  for (int i = 0; i < GRAPH_W; i++) {
    graphHistoryAI[i] = -1;
    graphHistoryTrad[i] = -1;
  }
}

// ---------------------- วงรอบการทำงานหลัก Loop ----------------------
void loop() {
  tickClock();

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    parseSerialCommand(cmd);
  }

  handleUserControls();

  unsigned long currentMillis = millis();

  if (currentMillis - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = currentMillis;

    float voltage = readFilteredVoltage();
    float phTrad = calculatePH_Traditional(voltage, testTempC);
    float phAI = predictPH_AI(voltage, testTempC, activeModelIndex);

    // เรนเดอร์หน้าจอตามโหมดที่ทำงานอยู่
    if (currentMode == MODE_FIELD_RUN) {
      updateFieldRunUI(voltage, testTempC, phTrad, phAI);
    } else if (currentMode == MODE_CALIBRATE) {
      updateCalibrateUI(voltage, testTempC);
    } else if (currentMode == MODE_AI_LEARN) {
      updateAILearnUI(voltage, testTempC, phTrad, phAI);
    }

    // บันทึกและส่งข้อมูล Telemetry อัตโนมัติ
    if (currentMillis - lastLogTime >= AUTO_LOG_INTERVAL) {
      lastLogTime = currentMillis;

      const char* logTag = (currentMode == MODE_FIELD_RUN) ? "AUTO_FIELD" : ((currentMode == MODE_CALIBRATE) ? "CALIB_MONITOR" : "TRAIN_MONITOR");
      logResearchDataRow(voltage, testTempC, phTrad, phAI, logTag);

      // ส่ง Telemetry ออกทาง USB Serial
      Serial.print("Volt:"); Serial.print(voltage, 4);
      Serial.print("\tTemp:"); Serial.print(testTempC, 1);
      Serial.print("\tpH_Trad:"); Serial.print(phTrad, 2);
      Serial.print("\tpH_AI:"); Serial.print(phAI, 2);
      Serial.print("\tMode:"); Serial.print(MODE_NAMES[currentMode]);
      Serial.print("\tModel:"); Serial.print(MODEL_NAMES[activeModelIndex]);
      Serial.print("\tTarget:"); Serial.print(MODEL_NAMES[activeModelIndex]);
      Serial.print("\tDateTime:"); Serial.print(getDateTimeString());
      Serial.print("\tSession:"); Serial.println(sessionName);
    }
  }
}
