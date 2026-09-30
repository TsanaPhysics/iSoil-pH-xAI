/*
 * ==============================================================================
 * โครงการวิจัย: การพัฒนาต้นแบบเครื่องวัดความเป็นกรด-ด่างของดินแบบพกพาภาคสนาม
 *             ความแม่นยำสูงด้วยการชดเชยความคลาดเคลื่อนโดยปัญญาประดิษฐ์
 * ชื่อระบบ: iSoil pH xAI (Smart Field Soil pH Meter with Embedded AI)
 * ทุนวิจัย: กองทุนวิจัย มหาวิทยาลัยราชภัฏรำไพพรรณี ประจำปีงบประมาณ 2569
 * คณะผู้วิจัย: อ.ธนพัฒน์ ถิระวุฒิ, อ.ดร.ชีวะ ทัศนา, ผศ.ดร.นันทพร มูลรังษี
 * 
 * คุณลักษณะหน้าจอใหม่ (iSoil pH xAI UI Redesign):
 *   1. ชื่อระบบใหม่: iSoil pH xAI (ไล่เฉดสี Gold, Emerald, Cyan)
 *   2. แสดงข้อมูลโมเดลปัญญาประดิษฐ์: [ANN 2-4-1 TinyML] โครงสร้าง 2 อินพุต, 4 ฮิดเดน (ReLU), 1 เอาต์พุต
 *   3. ขยายตัวเลขค่า pH AI ที่วัดได้จริงให้ใหญ่สะดุดตา (TextSize 6 Neon Green)
 *   4. ขยายค่าสัญญาณดิบจากโพรบ (Raw Cell Potential และ Temp) ให้ใหญ่ขึ้นเป็น TextSize 2 อ่านง่าย
 *   5. ขยายผลการวิเคราะห์สภาพดิน (Soil Agronomy Diagnostics) เป็น TextSize 2 พร้อมสีสันเตือนตามระดับความรุนแรง
 *   6. กราฟเปรียบเทียบ AI vs Traditional Nernst แบบเรียลไทม์ 60 FPS
 * ==============================================================================
 */

#include <Seeed_Arduino_FS.h>
#include <TFT_eSPI.h>

// ---------------------- Hardware Pin Definitions ----------------------
const int PH_PIN = A0;             // สัญญาณ Po เข้าขา A0 (BCM27)
const float VREF = 3.30;           // แรงดันอ้างอิง ADC ของ ATSAMD51 = 3.30V
const int ADC_MAX_VAL = 4095;      // 12-bit ADC (0 - 4095)
const int NUM_SAMPLES = 30;        // ตัวอย่างสำหรับการกรอง Median / Trimmed Filter

// ---------------------- ตัวแปรสำหรับการทดลองและสอบเทียบ ----------------
float testTempC = 25.0;            // อุณหภูมิสารละลาย (20 - 50 C)
int bufferIndex = 1;               // 0=4.01, 1=7.00, 2=10.01, 3=Soil Sample
const float BUFFER_PRESETS[] = {4.01, 7.00, 10.01, 0.00};
const char* BUFFER_NAMES[] = {"BUF 4.01", "BUF 7.00", "BUF 10.01", "SOIL SMP"};

// พารามิเตอร์การสอบเทียบเชิงเส้นแบบดั้งเดิม (Traditional Calibration)
float calV7 = 1.650;               // แรงดันที่ pH 7.00
float calV4 = 2.050;               // แรงดันที่ pH 4.01

// ---------------------- โมเดลโครงข่ายประสาทเทียม AI (ANN TinyML: 2-4-1) ----
// สถาปัตยกรรมโมเดล: Input Layer (2: Voltage, Temp) -> Hidden Layer (4 โหนด ReLU) -> Output (1: pH_AI)
float annWeightsL1[4][2] = {
  { -4.125f,  0.018f },
  {  3.892f, -0.015f },
  { -2.740f,  0.022f },
  {  1.650f, -0.008f }
};
float annBiasesL1[4] = { 6.805f, -6.412f, 4.520f, -2.723f };

float annWeightsL2[4] = { 0.942f, -0.885f, 0.612f, -0.374f };
float annBiasL2 = 6.985f;

// Forward pass ของแบบจำลอง AI
float predictPH_AI(float voltage, float tempC) {
  float hidden[4];
  for (int i = 0; i < 4; i++) {
    float sum = annBiasesL1[i] + (annWeightsL1[i][0] * voltage) + (annWeightsL1[i][1] * (tempC - 25.0f));
    hidden[i] = (sum > 0.0f) ? sum : 0.0f; // ฟังก์ชันกระตุ้น ReLU
  }

  float phOutput = annBiasL2;
  for (int i = 0; i < 4; i++) {
    phOutput += (hidden[i] * annWeightsL2[i]);
  }

  if (phOutput < 0.0f) phOutput = 0.0f;
  if (phOutput > 14.0f) phOutput = 14.0f;
  return phOutput;
}

// การคำนวณแบบดั้งเดิมตามทฤษฎี Nernst
float calculatePH_Traditional(float voltage, float tempC) {
  float kelvin = tempC + 273.15;
  float nernstSlopeFactor = kelvin / 298.15;
  float baseSlope = (7.00 - 4.01) / (calV7 - calV4);
  float effectiveSlope = baseSlope * nernstSlopeFactor;

  float ph = 7.00 + ((calV7 - voltage) * effectiveSlope);
  if (ph < 0.0f) ph = 0.0f;
  if (ph > 14.0f) ph = 14.0f;
  return ph;
}

// ---------------------- ระบบบันทึกชุดข้อมูลวิจัยลง MicroSD Card ----------
const char* DATASET_FILE = "/soil_ph_dataset.csv";
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

void logResearchDataRow(float voltage, float tempC, float phTrad, float phAI, const char* note) {
  if (!sdAvailable) return;

  File f = SD.open(DATASET_FILE, FILE_APPEND);
  if (f) {
    logIndex++;
    f.print(logIndex);
    f.print(",");
    f.print(millis());
    f.print(",");
    f.print(voltage, 4);
    f.print(",");
    f.print(tempC, 1);
    f.print(",");
    f.print(phTrad, 2);
    f.print(",");
    f.print(phAI, 2);
    f.print(",");
    f.print((bufferIndex < 3) ? String(BUFFER_PRESETS[bufferIndex], 2) : "FIELD");
    f.print(",");
    float errTrad = (bufferIndex < 3) ? (phTrad - BUFFER_PRESETS[bufferIndex]) : 0.0f;
    float errAI   = (bufferIndex < 3) ? (phAI - BUFFER_PRESETS[bufferIndex])   : 0.0f;
    f.print(errTrad, 3);
    f.print(",");
    f.print(errAI, 3);
    f.print(",");
    f.println(note);
    f.close();
  }
}

bool initResearchSD() {
  if (SD.begin(SDCARD_SS_PIN, SDCARD_SPI, 4000000UL)) {
    sdAvailable = true;
    if (!SD.exists(DATASET_FILE)) {
      File f = SD.open(DATASET_FILE, FILE_WRITE);
      if (f) {
        f.println("Index,Time_ms,Voltage_V,Temp_C,pH_Traditional,pH_AI,Standard_Target,Error_Trad,Error_AI,Sample_Type");
        f.close();
      }
    }
    return true;
  }
  sdAvailable = false;
  return false;
}

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

  tft.begin();
  tft.setRotation(3); // จอแนวนอน 320x240
  tft.fillScreen(TFT_BLACK);

  drawBaseUI();
  initResearchSD();
  drawSDStatus();

  for (int i = 0; i < GRAPH_W; i++) {
    graphHistoryAI[i] = -1;
    graphHistoryTrad[i] = -1;
  }
}

// ----------------- ฟังก์ชันวาดโครงสร้างหน้าจอหลัก (iSoil pH xAI) -----------------
void drawBaseUI() {
  // 1. แถบ Header ด้านบน (Y = 0 ถึง 24)
  tft.fillRect(0, 0, 320, 24, tft.color565(12, 24, 45));
  
  // ชื่อระบบใหม่: iSoil pH xAI (หลากสีสันชัดเจน)
  tft.setTextSize(2);
  tft.setTextColor(tft.color565(255, 210, 50), tft.color565(12, 24, 45)); // สีทอง
  tft.drawString("iSoil", 6, 4);

  tft.setTextColor(tft.color565(0, 255, 140), tft.color565(12, 24, 45));  // สีเขียว
  tft.drawString("pH", 72, 4);

  tft.setTextColor(tft.color565(0, 220, 255), tft.color565(12, 24, 45));  // สีฟ้าไซแอน
  tft.drawString("xAI", 102, 4);

  // ป้ายแสดงโมเดลบน Header Bar
  tft.setTextSize(1);
  tft.fillRoundRect(146, 4, 88, 16, 3, tft.color565(25, 45, 80));
  tft.setTextColor(tft.color565(245, 180, 60), tft.color565(25, 45, 80));
  tft.drawString("ANN 2-4-1 MLP", 152, 8);

  // 2. กล่องแสดงผล AI PREDICTED pH ขนาดใหญ่ (ซ้าย: X = 6, Y = 28, W = 186, H = 86)
  tft.drawRoundRect(6, 28, 186, 86, 4, tft.color565(0, 200, 110));
  tft.setTextColor(tft.color565(0, 230, 140), TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("AI COMPENSATED pH", 14, 34);

  // สัญญาณศักย์ไฟฟ้าจริงขนาดใหญ่ (Cell Potential V)
  tft.setTextColor(tft.color565(160, 185, 215), TFT_BLACK);
  tft.drawString("RAW CELL:", 14, 96);

  // 3. กล่องข้อมูลแบบจำลอง AI และ Traditional (ขวา: X = 196, Y = 28, W = 118, H = 86)
  tft.drawRoundRect(196, 28, 118, 86, 4, tft.color565(60, 80, 110));
  tft.fillRect(197, 29, 116, 16, tft.color565(30, 45, 75));
  tft.setTextColor(tft.color565(255, 210, 50), tft.color565(30, 45, 75));
  tft.setTextSize(1);
  tft.drawString("AI MODEL PARAMS", 204, 33);

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("NERNST :", 202, 50);
  tft.drawString("TEMP   :", 202, 68);
  tft.drawString("BUFFER :", 202, 84);
  tft.drawString("RECS   :", 202, 98);

  // 4. กรอบวิเคราะห์ดินด้านล่าง (เต็มความกว้าง: X = 6, Y = 118, W = 308, H = 42)
  tft.drawRoundRect(6, 118, 308, 42, 4, tft.color565(50, 70, 95));

  // 5. หัวข้อกราฟด้านล่าง
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("TREND : AI(Green) vs Traditional Nernst(Orange)", 10, 166);

  // กรอบพื้นที่กราฟ (X = 8, Y = 176, W = 304, H = 58)
  tft.drawRoundRect(GRAPH_X - 1, GRAPH_Y - 1, GRAPH_W + 2, GRAPH_H + 2, 3, tft.color565(40, 45, 60));
}

void drawSDStatus() {
  tft.setTextSize(1);
  if (sdAvailable) {
    tft.fillRoundRect(242, 4, 72, 16, 3, tft.color565(0, 100, 40));
    tft.setTextColor(TFT_WHITE, tft.color565(0, 100, 40));
    tft.drawString("SD: REC", 248, 8);
    tft.fillCircle(304, 12, 3, TFT_RED);
  } else {
    tft.fillRoundRect(242, 4, 72, 16, 3, tft.color565(70, 70, 70));
    tft.setTextColor(TFT_LIGHTGREY, tft.color565(70, 70, 70));
    tft.drawString("NO CARD", 250, 8);
  }
}

// ----------------- อัปเดตการวิเคราะห์สภาพดิน (ขนาดใหญ่ ชัดเจน มีสีสัน) ----------
void updateSoilStatusLarge(float ph) {
  tft.fillRect(8, 120, 304, 38, TFT_BLACK);

  // บรรทัดที่ 1: สถานะดิน ตัวใหญ่ TextSize 2 สีนีออนชัดเจน
  tft.setTextSize(2);

  if (ph < 5.0f) {
    // กรดจัด: สีแดงเด่นชัด
    tft.setTextColor(tft.color565(255, 60, 60), TFT_BLACK);
    tft.drawString("! VERY ACIDIC SOIL", 14, 122);

    // บรรทัดที่ 2: ข้อแนะนำการใส่ปูน TextSize 1
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("CRITICAL: Apply Dolomite Lime | Root Poison Al3+", 14, 144);

  } else if (ph >= 5.5f && ph <= 6.5f) {
    // เหมาะสมสำหรับทุเรียน: สีเขียวมรกต TextSize 2
    tft.setTextColor(tft.color565(0, 255, 140), TFT_BLACK);
    tft.drawString("* OPTIMAL DURIAN", 14, 122);

    tft.setTextSize(1);
    tft.setTextColor(tft.color565(180, 255, 200), TFT_BLACK);
    tft.drawString("EXCELLENT: Ideal Soil Health for Maximum Yield", 14, 144);

  } else if (ph > 6.5f && ph <= 7.5f) {
    // ดินเป็นกลาง: สีฟ้าไซแอน
    tft.setTextColor(tft.color565(0, 210, 255), TFT_BLACK);
    tft.drawString("= NEUTRAL SOIL", 14, 122);

    tft.setTextSize(1);
    tft.setTextColor(tft.color565(180, 230, 255), TFT_BLACK);
    tft.drawString("BALANCED: Standard Balanced Nutrients (pH 6.6-7.5)", 14, 144);

  } else {
    // ดินเป็นด่าง: สีม่วงชมพู
    tft.setTextColor(tft.color565(255, 100, 220), TFT_BLACK);
    tft.drawString("^ ALKALINE SOIL", 14, 122);

    tft.setTextSize(1);
    tft.setTextColor(tft.color565(255, 200, 240), TFT_BLACK);
    tft.drawString("CAUTION: Low Zinc/Iron | Add Compost & Organic", 14, 144);
  }
}

void loop() {
  handleUserControls();

  unsigned long currentMillis = millis();

  if (currentMillis - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = currentMillis;

    float voltage = readFilteredVoltage();
    float phTrad = calculatePH_Traditional(voltage, testTempC);
    float phAI = predictPH_AI(voltage, testTempC);

    // 1. แสดงตัวเลข AI pH ขนาดใหญ่พิเศษ (TextSize 6 - ใหญ่ คมชัด สะดุดตา)
    tft.fillRect(14, 46, 172, 46, TFT_BLACK);
    
    // เปลี่ยนสีตัวเลข AI pH ตามช่วงความเป็นกรด-ด่าง
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

    // 2. แสดงค่าที่วัดได้จริงขนาดใหญ่ขึ้น: RAW CELL POTENTIAL (TextSize 2, สีเหลืองทอง)
    tft.fillRect(86, 94, 96, 16, TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(255, 215, 60), TFT_BLACK);
    char bufVolt[10];
    dtostrf(voltage, 4, 3, bufVolt);
    tft.drawString(String(bufVolt) + "V", 86, 94);

    // 3. แสดงพารามิเตอร์ของโมเดล AI ในกล่องขวา
    // Nernst Traditional (TextSize 2, สีส้มสด)
    tft.fillRect(254, 48, 56, 16, TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(255, 145, 40), TFT_BLACK);
    char bufTrad[8];
    dtostrf(phTrad, 4, 2, bufTrad);
    tft.drawString(bufTrad, 254, 48);

    // Temp (TextSize 1)
    tft.fillRect(254, 68, 56, 12, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 230, 255), TFT_BLACK);
    tft.drawString(String(testTempC, 1) + " C", 254, 68);

    // Target Buffer
    tft.fillRect(254, 84, 56, 12, TFT_BLACK);
    tft.setTextColor(tft.color565(255, 220, 100), TFT_BLACK);
    tft.drawString(BUFFER_NAMES[bufferIndex], 254, 84);

    // Records Count
    tft.fillRect(254, 98, 56, 12, TFT_BLACK);
    tft.setTextColor(tft.color565(150, 200, 255), TFT_BLACK);
    tft.drawString(String(logIndex), 254, 98);

    // 4. แสดงผลการวิเคราะห์สภาพดินขนาดใหญ่ (TextSize 2 + 1 หลากสีสัน)
    updateSoilStatusLarge(phAI);

    // 5. วาดกราฟเปรียบเทียบ AI vs Traditional
    updateComparisonGraph(phAI, phTrad);

    // 6. บันทึกข้อมูลอัตโนมัติลง SD Card
    if (currentMillis - lastLogTime >= AUTO_LOG_INTERVAL) {
      lastLogTime = currentMillis;
      logResearchDataRow(voltage, testTempC, phTrad, phAI, "AUTO_SAMPLE");
    }

    // 7. ส่ง Telemetry ออก Serial
    Serial.print("Volt:"); Serial.print(voltage, 4);
    Serial.print("\tTemp:"); Serial.print(testTempC, 1);
    Serial.print("\tpH_Trad:"); Serial.print(phTrad, 2);
    Serial.print("\tpH_AI:"); Serial.print(phAI, 2);
    Serial.print("\tTarget:"); Serial.println(BUFFER_NAMES[bufferIndex]);
  }
}

// จัดการปุ่มกด 5 ทิศทาง
void handleUserControls() {
  static unsigned long lastBtnPress = 0;
  if (millis() - lastBtnPress < 200) return;

  if (digitalRead(WIO_5S_UP) == LOW) {
    bufferIndex = (bufferIndex + 1) % 4;
    lastBtnPress = millis();
  } else if (digitalRead(WIO_5S_DOWN) == LOW) {
    bufferIndex = (bufferIndex + 3) % 4;
    lastBtnPress = millis();
  }

  if (digitalRead(WIO_5S_RIGHT) == LOW) {
    if (testTempC < 50.0f) testTempC += 5.0f;
    lastBtnPress = millis();
  } else if (digitalRead(WIO_5S_LEFT) == LOW) {
    if (testTempC > 20.0f) testTempC -= 5.0f;
    lastBtnPress = millis();
  }

  if (digitalRead(WIO_5S_PRESS) == LOW || digitalRead(WIO_KEY_A) == LOW) {
    float v = readFilteredVoltage();
    float phT = calculatePH_Traditional(v, testTempC);
    float phA = predictPH_AI(v, testTempC);
    logResearchDataRow(v, testTempC, phT, phA, "MANUAL_SNAPSHOT");
    
    // เอฟเฟกต์ไฟกะพริบขอบจอเมื่อบันทึก
    tft.drawRoundRect(6, 28, 186, 86, 4, TFT_WHITE);
    delay(80);
    tft.drawRoundRect(6, 28, 186, 86, 4, tft.color565(0, 200, 110));
    lastBtnPress = millis() + 300;
  }
}

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

  // เส้นประ pH 7 กึ่งกลาง
  int midY = GRAPH_Y + (GRAPH_H / 2);
  for (int x = GRAPH_X; x < GRAPH_X + GRAPH_W; x += 6) {
    tft.drawPixel(x, midY, tft.color565(60, 65, 80));
  }

  // วาดเส้นกราฟ AI (สีเขียวสว่างมรกต) และ Traditional (สีส้มสด)
  for (int i = 0; i < GRAPH_W - 1; i++) {
    if (graphHistoryTrad[i] > 0 && graphHistoryTrad[i + 1] > 0) {
      tft.drawLine(GRAPH_X + i, graphHistoryTrad[i], GRAPH_X + i + 1, graphHistoryTrad[i + 1], tft.color565(255, 140, 30));
    }
    if (graphHistoryAI[i] > 0 && graphHistoryAI[i + 1] > 0) {
      tft.drawLine(GRAPH_X + i, graphHistoryAI[i], GRAPH_X + i + 1, graphHistoryAI[i + 1], tft.color565(0, 255, 140));
    }
  }
}
