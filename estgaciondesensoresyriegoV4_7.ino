// ==========================================================================================
// ===== CÓDIGO EMISOR (SENSORES) - V4.9.5 - CORRECCIÓN FINALÍSIMA DE CALLBACKS =========
// ==========================================================================================

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <EEPROM.h>
#include <SPIFFS.h>

#include <WiFiUdp.h>
#include <NTPClient.h>

// !! IMPORTANTE: ESTA ES LA DIRECCIÓN MAC DEL RECEPTOR (T-DISPLAY) !!
uint8_t broadcastAddress[] = {0x8C, 0x4F, 0x00, 0xD3, 0xAE, 0xB4};

// Estructura de datos para enviar. DEBE SER IDÉNTICA en ambos ESP32.
typedef struct struct_message {
    int humedad1;
    int humedad2;
    int humedadZonaBaja1;
    int humedadZonaBaja2;
    bool regando1;
    bool regando2;
    float phCalculado;
} struct_message;

// Estructura para recibir comandos. DEBE SER IDÉNTICA en ambos ESP32.
typedef struct struct_command {
    int bombaId; // 1 o 2
    bool activar;
} struct_command;

// Variable para guardar los datos a enviar y información del peer.
struct_message datosParaEnviar;
esp_now_peer_info_t peerInfo;

// ==================== AJUSTE DE ZONA HORARIA ====================
#define UTC_OFFSET_SECONDS -21600 // UTC-6 (HORARIO ESTÁNDAR DE MÉXICO CENTRAL)

// ==================== CONFIGURACIÓN WIFI ====================
const char* ap_ssid = "ESP32_Riego";
WiFiServer server(80);
const char* WIFI_SSID = "IZZI-D1A8";
const char* WIFI_PASSWORD = "HJ39R2YLMJ9H";
WiFiUDP ntpUdp;
NTPClient timeClient(ntpUdp, "pool.ntp.org", UTC_OFFSET_SECONDS, 60000);

// ==================== PINES DE SENSOR ====================
const int sensorPin1 = 35;
const int sensorPin2 = 32;
const int relePin1   = 27;
const int relePin2   = 14;
const int ledWifi    = 2;
const int SENSOR_ZONA_BAJA1 = 36;
const int SENSOR_ZONA_BAJA2 = 39;

// ==================== PANTALLA TFT (SPI) ====================
#define TFT_CS    15
#define TFT_RST   4
#define TFT_DC    5
#define TFT_SCLK  18
#define TFT_MOSI  23
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ==================== I2C / RTC ====================
#define I2C_SDA   22
#define I2C_SCL   21
#define SQW_PIN   19
RTC_DS3231 rtc;

// ==================== SENSOR PH ====================
#define PH_ANALOG_PIN 34
#define PH_DIGITAL_PIN 16
#define PH_TEMP_PIN 17

// ==================== JOYSTICK / BOTONES ====================
#define JOY_X     17
#define JOY_Y     33
#define JOY_SW    26
#define BTN_TAB   12
#define BTN_OK    13

// ==================== VALORES FIJOS DE CALIBRACIÓN ====================
const int humedad1DryRaw = 3260;
const int humedad1WetRaw = 1200;
const int humedad2DryRaw = 3270;
const int humedad2WetRaw = 1100;
const int humedadZonaBaja1DryRaw = 3219;
const int humedadZonaBaja1WetRaw = 1099;
const int humedadZonaBaja2DryRaw = 3219;
const int humedadZonaBaja2WetRaw = 1107;

// ==================== CONTROL ====================
int humedadInicio;
int humedadFinal;
#define ADDR_HUMEDAD_INICIO 0
#define ADDR_HUMEDAD_FINAL  1
bool regando1 = false;
bool regando2 = false;
int humedad1 = 0;
int humedad2 = 0;
int humedadZonaBaja1 = 0;
int humedadZonaBaja2 = 0;
bool mostrarHora = true;

// --- VARIABLES DE ESTADO AVANZADAS ---
bool runoffZona1 = false;
bool runoffZona2 = false;
bool manualWateringActive = false;
int manualWateringZone = 0;
unsigned long manualWateringEndTime = 0;
const unsigned long MANUAL_WATERING_DURATION = 20000;

// ==================== VARIABLES PARA FERTIRRIEGO (MODIFICADO) ====================
bool fertiriegoActivo = false;
unsigned long inicioCicloFertiriego = 0;
unsigned long cambioBombaFertirriego = 0;
enum FertiriegoState { FERTIRIEGO_INACTIVO, FERTIRIEGO_BOMBA1, FERTIRIEGO_BOMBA2, FERTIRIEGO_ESPERA };
FertiriegoState estadoFertiriego = FERTIRIEGO_INACTIVO;
const unsigned long FERTIRIEGO_INTERVALO = 40 * 60 * 1000UL; // 40 minutos
const unsigned long FERTIRIEGO_DURACION_BOMBA = 5 * 1000UL;  // 5 segundos por bomba


// --- REPOSO ---
unsigned long lastInteractionTime = 0;
bool screenIsAsleep = false;
const unsigned long SCREEN_SLEEP_TIMEOUT = 60000;

// ==================== ESTADOS Y NAVEGACIÓN DEL MENÚ ====================
enum MenuState {
  STATE_MAIN_DISPLAY, STATE_MENU_NAVIGATE, STATE_RUNOFF_SETTINGS,
  STATE_HISTORY_CALENDAR, STATE_HISTORY_DISPLAY, STATE_PH_DISPLAY,
  STATE_THRESHOLD_SETTINGS, STATE_MANUAL_WATER_MENU, STATE_MANUAL_WATER_ACTIVE
};
MenuState currentMenuState = STATE_MAIN_DISPLAY;
int selectedMainMenuOption = 0; int selectedSubMenuOption = 0;
int selectedThresholdOption = 0; bool isEditingThreshold = false;

// --- OPCIONES DE MENÚ ACTUALIZADAS ---
const int MENU_RIEGO_MANUAL = 1;
const int MENU_FERTIRRIEGO  = 2;
const int MENU_RUNOFF       = 3;
const int MENU_HISTORIA     = 4;
const int MENU_PH           = 5;
const int MENU_AJUSTES      = 6;
const int MENU_TOTAL        = 6;

bool screenNeedsRedraw = true;
int phValorAnalogico = 0; bool phAlto = false; float phVoltaje = 0.0;
float phCalculado = 0.0; const float phOffset = 0.0;
unsigned long ultimoChequeo = 0; unsigned long ultimoPrint   = 0;
unsigned long lastPhScreenUpdateTime = 0; const unsigned long phScreenUpdateInterval = 1000;
const unsigned long intervaloChequeo = 3000; const unsigned long intervaloPrint = 1000;
int hourlyHumidity1[24], hourlyHumidity2[24], hourlyHumidityZonaBaja1[24], hourlyHumidityZonaBaja2[24], hourlyReadingsCount[24];
int currentDay = -1; int currentMonth = -1;

DateTime calendarDate;
bool noHistoryDataForDay = false;
char historyDateStr[11] = "Hoy";


// ====================================================================================
// --- ¡¡AQUÍ ESTÁ LA CORRECCIÓN PARA EL ERROR DE COMPILACIÓN!! ---
// ====================================================================================

// --- ¡¡CORRECCIÓN DEFINITIVA!! -> La firma de la función ha sido actualizada ---
// Esta es la firma que tu versión de la librería ESP32 espera para el callback de envío.
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("DIAGNÓSTICO (Emisor): Estado del envío de datos: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Éxito" : "Fallo");
}

// --- ¡¡CORRECCIÓN DEFINITIVA!! -> La firma de la función ha sido actualizada ---
// Esta es la firma que tu versión de la librería ESP32 espera para el callback de recepción.
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  struct_command comandoRecibido;
  if (len == sizeof(comandoRecibido)) {
    memcpy(&comandoRecibido, incomingData, sizeof(comandoRecibido));
    Serial.printf("DIAGNÓSTICO (Emisor): Comando remoto para bomba %d, activar: %s\n",
                  comandoRecibido.bombaId, comandoRecibido.activar ? "SI" : "NO");

    if (fertiriegoActivo) {
        Serial.println("Comando ignorado: Modo Fertiriego activo.");
        return;
    }

    if (comandoRecibido.bombaId == 1) {
      if (!runoffZona1 && !manualWateringActive) {
        regando1 = comandoRecibido.activar;
        digitalWrite(relePin1, regando1 ? LOW : HIGH);
        screenNeedsRedraw = true;
      }
    }
    else if (comandoRecibido.bombaId == 2) {
      if (!runoffZona2 && !manualWateringActive) {
        regando2 = comandoRecibido.activar;
        digitalWrite(relePin2, regando2 ? LOW : HIGH);
        screenNeedsRedraw = true;
      }
    }
  }
}

// ====================================================================================


void enviarDatosPorEspNow() {
  datosParaEnviar.humedad1 = humedad1;
  datosParaEnviar.humedad2 = humedad2;
  datosParaEnviar.humedadZonaBaja1 = humedadZonaBaja1;
  datosParaEnviar.humedadZonaBaja2 = humedadZonaBaja2;
  datosParaEnviar.regando1 = regando1;
  datosParaEnviar.regando2 = regando2;
  datosParaEnviar.phCalculado = phCalculado;
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &datosParaEnviar, sizeof(datosParaEnviar));
  if (result != ESP_OK) {
    Serial.println("Error enviando los datos por ESP-NOW");
  }
}

void drawMainMenuOption(const char* label, bool isSelected, bool isActiveState, int yPos) {
  uint16_t textColor = ST77XX_WHITE;
  uint16_t bgColor = ST77XX_BLACK;
  if (isSelected) {
    textColor = ST77XX_BLACK;
    bgColor = ST77XX_WHITE;
    if (strncmp(label, "RIEGO MANUAL", 12) == 0) bgColor = ST77XX_BLUE;
    else if (strncmp(label, "FERTIRRIEGO", 11) == 0) bgColor = isActiveState ? ST77XX_MAGENTA : ST77XX_WHITE;
    else if (strncmp(label, "RUNOFF", 6) == 0) bgColor = isActiveState ? ST77XX_YELLOW : ST77XX_WHITE;
    else if (strncmp(label, "HISTORIA", 8) == 0) bgColor = ST77XX_MAGENTA;
    else if (strncmp(label, "pH", 2) == 0) bgColor = ST77XX_CYAN;
    else if (strncmp(label, "AJUSTES", 7) == 0) bgColor = ST77XX_ORANGE;
  }
  tft.fillRect(0, yPos - 2, tft.width(), 18, bgColor);
  tft.setTextColor(textColor);
  tft.setTextSize(2);
  int textWidth = strlen(label) * 12;
  int textX = (tft.width() - textWidth) / 2;
  tft.setCursor(textX, yPos);
  tft.print(label);
}

void drawSubMenuOption(const char* label, bool isSelected, bool isActive, int yPos) {
    uint16_t textColor = ST77XX_WHITE;
    uint16_t bgColor = ST77XX_BLACK;
    if (isSelected) {
        bgColor = ST77XX_WHITE;
        textColor = ST77XX_BLACK;
    }
    char buffer[25];
    if (currentMenuState == STATE_RUNOFF_SETTINGS) {
        snprintf(buffer, sizeof(buffer), "%s [%s]", label, isActive ? "ON" : "OFF");
        if (isActive && !isSelected) textColor = ST77XX_GREEN;
    } else {
        strncpy(buffer, label, sizeof(buffer));
    }
    tft.fillRect(0, yPos - 2, tft.width(), 18, bgColor);
    tft.setTextColor(textColor);
    tft.setTextSize(2);
    int textWidth = strlen(buffer) * 12;
    int textX = (tft.width() - textWidth) / 2;
    tft.setCursor(textX, yPos);
    tft.print(buffer);
}

void drawThresholdOption(const char* label, int value, bool isSelected, bool isEditing, int yPos) {
  uint16_t textColor;
  uint16_t bgColor;
  if (isSelected) {
    bgColor = isEditing ? ST77XX_YELLOW : ST77XX_WHITE;
    textColor = ST77XX_BLACK;
  } else {
    bgColor = ST77XX_BLACK;
    textColor = ST77XX_WHITE;
  }
  tft.fillRect(0, yPos, tft.width(), 20, bgColor);
  tft.setTextColor(textColor);
  tft.setTextSize(2);
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%s %d%%", label, value);
  int textWidth = strlen(buffer) * 12;
  int textX = (tft.width() - textWidth) / 2;
  tft.setCursor(textX, yPos + 2);
  tft.print(buffer);
}

uint16_t colorPH(float ph) {
  int i = round(ph);
  if (i == 7) return ST77XX_WHITE;
  else if (i == 6 || i == 8) return 0x07E0;
  else if (i >= 4 && i <= 5) return 0xFD20;
  else if (i >= 1 && i <= 3) return ST77XX_RED;
  else if (i >= 9 && i <= 11) return ST77XX_BLUE;
  else if (i >= 12 && i <= 14) return 0x780F;
  return ST77XX_WHITE;
}

void saveHistory() {
    DateTime now = rtc.now();
    DateTime yesterday = now - TimeSpan(1, 0, 0, 0);
    char filename[20];
    sprintf(filename, "/hist/%04d%02d%02d.dat", yesterday.year(), yesterday.month(), yesterday.day());
    File file = SPIFFS.open(filename, FILE_WRITE);
    if (!file) {
        Serial.printf("Error al abrir archivo de historial para escritura: %s\n", filename);
        return;
    }
    file.write((uint8_t*)hourlyHumidity1, sizeof(hourlyHumidity1));
    file.write((uint8_t*)hourlyHumidity2, sizeof(hourlyHumidity2));
    file.write((uint8_t*)hourlyHumidityZonaBaja1, sizeof(hourlyHumidityZonaBaja1));
    file.write((uint8_t*)hourlyHumidityZonaBaja2, sizeof(hourlyHumidityZonaBaja2));
    file.write((uint8_t*)hourlyReadingsCount, sizeof(hourlyReadingsCount));
    file.close();
    Serial.printf("Historial del día %02d/%02d guardado en %s\n", yesterday.day(), yesterday.month(), filename);
}

bool loadHistory(DateTime dateToLoad) {
    char filename[20];
    sprintf(filename, "/hist/%04d%02d%02d.dat", dateToLoad.year(), dateToLoad.month(), dateToLoad.day());
    if (!SPIFFS.exists(filename)) {
        Serial.printf("Archivo de historial no encontrado: %s\n", filename);
        memset(hourlyHumidity1, 0, sizeof(hourlyHumidity1));
        memset(hourlyHumidity2, 0, sizeof(hourlyHumidity2));
        memset(hourlyHumidityZonaBaja1, 0, sizeof(hourlyHumidityZonaBaja1));
        memset(hourlyHumidityZonaBaja2, 0, sizeof(hourlyHumidityZonaBaja2));
        memset(hourlyReadingsCount, 0, sizeof(hourlyReadingsCount));
        return false;
    }
    File file = SPIFFS.open(filename, FILE_READ);
    if (!file) {
        Serial.printf("Error al abrir archivo de historial para lectura: %s\n", filename);
        return false;
    }
    file.read((uint8_t*)hourlyHumidity1, sizeof(hourlyHumidity1));
    file.read((uint8_t*)hourlyHumidity2, sizeof(hourlyHumidity2));
    file.read((uint8_t*)hourlyHumidityZonaBaja1, sizeof(hourlyHumidityZonaBaja1));
    file.read((uint8_t*)hourlyHumidityZonaBaja2, sizeof(hourlyHumidityZonaBaja2));
    file.read((uint8_t*)hourlyReadingsCount, sizeof(hourlyReadingsCount));
    file.close();
    Serial.printf("Historial cargado para el día %02d/%02d/%04d\n", dateToLoad.day(), dateToLoad.month(), dateToLoad.year());
    return true;
}

void readSensorsAndControlRelays() {
  int raw1 = analogRead(sensorPin1);
  int raw2 = analogRead(sensorPin2);
  int rawZonaBaja1 = analogRead(SENSOR_ZONA_BAJA1);
  int rawZonaBaja2 = analogRead(SENSOR_ZONA_BAJA2);
  humedad1 = map(raw1, humedad1DryRaw, humedad1WetRaw, 0, 100);
  humedad2 = map(raw2, humedad2DryRaw, humedad2WetRaw, 0, 100);
  humedadZonaBaja1 = map(rawZonaBaja1, humedadZonaBaja1DryRaw, humedadZonaBaja1WetRaw, 0, 100);
  humedadZonaBaja2 = map(rawZonaBaja2, humedadZonaBaja2DryRaw, humedadZonaBaja2WetRaw, 0, 100);
  humedad1 = constrain(humedad1, 0, 100);
  humedad2 = constrain(humedad2, 0, 100);
  humedadZonaBaja1 = constrain(humedadZonaBaja1, 0, 100);
  humedadZonaBaja2 = constrain(humedadZonaBaja2, 0, 100);

  if (!manualWateringActive && !runoffZona1 && !fertiriegoActivo) {
    if (humedad1 < humedadInicio && !regando1) {
      digitalWrite(relePin1, LOW);
      regando1 = true;
    } else if (humedad1 >= humedadFinal && regando1) {
      digitalWrite(relePin1, HIGH);
      regando1 = false;
    }
  }
  if (!manualWateringActive && !runoffZona2 && !fertiriegoActivo) {
    if (humedad2 < humedadInicio && !regando2) {
      digitalWrite(relePin2, LOW);
      regando2 = true;
    } else if (humedad2 >= humedadFinal && regando2) {
      digitalWrite(relePin2, HIGH);
      regando2 = false;
    }
  }

  phValorAnalogico = analogRead(PH_ANALOG_PIN);
  phAlto = digitalRead(PH_DIGITAL_PIN);
  phVoltaje = (phValorAnalogico / 4095.0) * 3.3;
  phCalculado = 7 + ((2.5 - phVoltaje) / 0.18) + phOffset;
  DateTime now = rtc.now();
  int currentHour = now.hour();
  if (now.day() != currentDay || now.month() != currentMonth) {
    saveHistory();
    memset(hourlyHumidity1, 0, sizeof(hourlyHumidity1));
    memset(hourlyHumidity2, 0, sizeof(hourlyHumidity2));
    memset(hourlyHumidityZonaBaja1, 0, sizeof(hourlyHumidityZonaBaja1));
    memset(hourlyHumidityZonaBaja2, 0, sizeof(hourlyHumidityZonaBaja2));
    memset(hourlyReadingsCount, 0, sizeof(hourlyReadingsCount));
    currentDay = now.day();
    currentMonth = now.month();
  }
  hourlyHumidity1[currentHour] += humedad1;
  hourlyHumidity2[currentHour] += humedad2;
  hourlyHumidityZonaBaja1[currentHour] += humedadZonaBaja1;
  hourlyHumidityZonaBaja2[currentHour] += humedadZonaBaja2;
  hourlyReadingsCount[currentHour]++;
}

void drawCalendar() {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    char monthYearStr[20];
    const char* months[] = {"Ene", "Feb", "Mar", "Abr", "May", "Jun", "Jul", "Ago", "Sep", "Oct", "Nov", "Dic"};
    sprintf(monthYearStr, "%s %d", months[calendarDate.month() - 1], calendarDate.year());
    tft.setCursor((tft.width() - strlen(monthYearStr) * 12) / 2, 10);
    tft.print(monthYearStr);
    int cellWidth = 32;
    int cellHeight = 25;
    int startX = (tft.width() - 7 * cellWidth) / 2;
    int headerY = 40;
    int startY = headerY + 20;
    const char* daysOfWeek[] = {"L", "M", "M", "J", "V", "S", "D"};
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW);
    for(int i=0; i<7; i++){
        tft.setCursor(startX + (i * cellWidth) + (cellWidth / 2 - 6), headerY);
        tft.print(daysOfWeek[i]);
    }
    DateTime firstDayOfMonth(calendarDate.year(), calendarDate.month(), 1);
    uint8_t dayOfWeek = firstDayOfMonth.dayOfTheWeek();
    dayOfWeek = (dayOfWeek == 0) ? 6 : dayOfWeek - 1;
    DateTime today = rtc.now();
    for (int day = 1; day <= 31; ++day) {
        DateTime currentDay(calendarDate.year(), calendarDate.month(), day);
        if (currentDay.month() != calendarDate.month()) break;
        int col = (dayOfWeek + day - 1) % 7;
        int row = (dayOfWeek + day - 1) / 7;
        int x = startX + col * cellWidth;
        int y = startY + row * cellHeight;
        uint16_t textColor = ST77XX_WHITE;
        char filename[20];
        sprintf(filename, "/hist/%04d%02d%02d.dat", currentDay.year(), currentDay.month(), currentDay.day());
        if (SPIFFS.exists(filename)) {
            textColor = ST77XX_CYAN;
        }
        if (currentDay.day() == calendarDate.day() && currentDay.month() == calendarDate.month() && currentDay.year() == calendarDate.year()) {
            tft.fillRect(x, y, cellWidth - 2, cellHeight - 2, ST77XX_WHITE);
            textColor = ST77XX_BLACK;
        } else if (currentDay.day() == today.day() && currentDay.month() == today.month() && currentDay.year() == today.year()) {
             tft.drawRect(x, y, cellWidth - 2, cellHeight - 2, ST77XX_YELLOW);
        }
        tft.setTextColor(textColor);
        tft.setTextSize(2);
        tft.setCursor(x + (day < 10 ? 9 : 0), y + 4);
        tft.print(day);
    }
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, tft.height() - 15);
    tft.print("SW:Ver|OK:Der|TAB:Izq|Y:Sem");
}

void drawCurrentScreenState() {
  if (screenIsAsleep) {
    return;
  }
  tft.fillScreen(ST77XX_BLACK);
  int currentY = 5;
  const int min_gap_between_zones = 10;
  switch(currentMenuState) {
    case STATE_MANUAL_WATER_ACTIVE: {
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(2);
      char title[20];
      sprintf(title, "REGANDO ZONA %d", manualWateringZone);
      tft.setCursor((tft.width() - strlen(title) * 12) / 2, 40);
      tft.print(title);
      unsigned long remainingMillis = manualWateringEndTime > millis() ? manualWateringEndTime - millis() : 0;
      int remainingSeconds = remainingMillis / 1000;
      tft.setTextColor(ST77XX_CYAN);
      tft.setTextSize(7);
      char timeStr[4];
      sprintf(timeStr, "%02d", remainingSeconds);
      tft.setCursor((tft.width() - 2 * 42) / 2, 100);
      tft.print(timeStr);
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(1);
      const char* instructions = "OK o TAB para Cancelar";
      tft.setCursor((tft.width() - strlen(instructions) * 6) / 2, tft.height() - 20);
      tft.print(instructions);
      break;
    }
    case STATE_HISTORY_CALENDAR:
      drawCalendar();
      break;
    case STATE_HISTORY_DISPLAY: {
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(1);
      char title[25];
      sprintf(title, "HISTORIAL: %s", historyDateStr);
      tft.setCursor(tft.width() / 2 - (strlen(title) * 6) / 2, 5);
      tft.print(title);
      if(noHistoryDataForDay) {
        tft.setTextColor(ST77XX_YELLOW);
        tft.setTextSize(2);
        tft.setCursor(20, 100);
        tft.print("No hay datos para");
        tft.setCursor(40, 120);
        tft.print("este dia.");
        tft.setTextColor(ST77XX_WHITE);
        tft.setTextSize(1);
        tft.setCursor(tft.width() / 2 - (strlen("Regresar (SW)")*6)/2, tft.height() - 15);
        tft.print("Regresar (SW)");
        break;
      }
      int graphOriginX = 25;
      int graphOriginY = tft.height() - 30;
      int graphWidth = tft.width() - graphOriginX - 10;
      int graphHeight = 150;
      tft.drawLine(graphOriginX, graphOriginY, graphOriginX + graphWidth, graphOriginY, ST77XX_WHITE);
      tft.drawLine(graphOriginX, graphOriginY, graphOriginX, graphOriginY - graphHeight, ST77XX_WHITE);
      tft.setCursor(0, graphOriginY - graphHeight + 5);
      tft.print("100%");
      tft.setCursor(0, graphOriginY - 5);
      tft.print("0%");
      int labelHours[] = {0, 6, 12, 18};
      for (int i = 0; i < sizeof(labelHours) / sizeof(labelHours[0]); i++) {
          int h_label = labelHours[i];
          int x_pos = map(h_label, 0, 23, 0, graphWidth);
          tft.setCursor(graphOriginX + x_pos - (h_label < 10 ? 3 : 6), graphOriginY + 5);
          tft.printf("%02d", h_label);
      }
      tft.setCursor(graphOriginX + graphWidth - 10, graphOriginY + 5);
      tft.print("23");
      for (int h = 0; h < 24; h++) {
        if (hourlyReadingsCount[h] > 0) {
          int avgHumi1 = hourlyHumidity1[h] / hourlyReadingsCount[h];
          int avgHumi2 = hourlyHumidity2[h] / hourlyReadingsCount[h];
          int avgHumiZB1 = hourlyHumidityZonaBaja1[h] / hourlyReadingsCount[h];
          int avgHumiZB2 = hourlyHumidityZonaBaja2[h] / hourlyReadingsCount[h];
          int x_curr = map(h, 0, 23, 0, graphWidth) + graphOriginX;
          int y1_curr = graphOriginY - map(avgHumi1, 0, 100, 0, graphHeight);
          int y2_curr = graphOriginY - map(avgHumi2, 0, 100, 0, graphHeight);
          int yZB1_curr = graphOriginY - map(avgHumiZB1, 0, 100, 0, graphHeight);
          int yZB2_curr = graphOriginY - map(avgHumiZB2, 0, 100, 0, graphHeight);
          if (h > 0 && hourlyReadingsCount[h-1] > 0) {
            int avgHumi1_prev = hourlyHumidity1[h-1] / hourlyReadingsCount[h-1];
            int avgHumi2_prev = hourlyHumidity2[h-1] / hourlyReadingsCount[h-1];
            int avgHumiZB1_prev = hourlyHumidityZonaBaja1[h-1] / hourlyReadingsCount[h-1];
            int avgHumiZB2_prev = hourlyHumidityZonaBaja2[h-1] / hourlyReadingsCount[h-1];
            int x_prev = map(h-1, 0, 23, 0, graphWidth) + graphOriginX;
            int y1_prev = graphOriginY - map(avgHumi1_prev, 0, 100, 0, graphHeight);
            int y2_prev = graphOriginY - map(avgHumi2_prev, 0, 100, 0, graphHeight);
            int yZB1_prev = graphOriginY - map(avgHumiZB1_prev, 0, 100, 0, graphHeight);
            int yZB2_prev = graphOriginY - map(avgHumiZB2_prev, 0, 100, 0, graphHeight);
            tft.drawLine(x_prev, y1_prev, x_curr, y1_curr, ST77XX_BLUE);
            tft.drawLine(x_prev, y2_prev, x_curr, y2_curr, ST77XX_GREEN);
            tft.drawLine(x_prev, yZB1_prev, x_curr, yZB1_curr, ST77XX_ORANGE);
            tft.drawLine(x_prev, yZB2_prev, x_curr, yZB2_curr, ST77XX_YELLOW);
          } else {
            tft.fillCircle(x_curr, y1_curr, 2, ST77XX_BLUE);
            tft.fillCircle(x_curr, y2_curr, 2, ST77XX_GREEN);
            tft.fillCircle(x_curr, yZB1_curr, 2, ST77XX_ORANGE);
            tft.fillCircle(x_curr, yZB2_curr, 2, ST77XX_YELLOW);
          }
        }
      }
      tft.setTextColor(ST77XX_BLUE); tft.setCursor(graphOriginX, graphOriginY - graphHeight - 15); tft.print("Z1");
      tft.setTextColor(ST77XX_GREEN); tft.setCursor(graphOriginX + 25, graphOriginY - graphHeight - 15); tft.print("Z2");
      tft.setTextColor(ST77XX_ORANGE); tft.setCursor(graphOriginX + 50, graphOriginY - graphHeight - 15); tft.print("ZB1");
      tft.setTextColor(ST77XX_YELLOW); tft.setCursor(graphOriginX + 80, graphOriginY - graphHeight - 15); tft.print("ZB2");
      tft.setTextColor(ST77XX_WHITE); tft.setCursor(tft.width() / 2 - (strlen("Regresar (SW)")*6)/2, tft.height() - 15); tft.print("Regresar (SW)");
      break;
    }
    case STATE_PH_DISPLAY: {
      tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2); tft.setCursor(tft.width() / 2 - (strlen("VALOR pH") * 12) / 2, 10); tft.print("VALOR pH");
      tft.setTextSize(4); tft.setTextColor(colorPH(phCalculado));
      char phText[10]; snprintf(phText, sizeof(phText), "%.2f", phCalculado);
      tft.setCursor(tft.width() / 2 - (strlen(phText) * 24) / 2, 50); tft.print(phText);
      int scaleX = tft.width() / 2 - 10;
      int scaleYStart = 100;
      int scaleHeight = tft.height() - scaleYStart - 40;
      tft.drawRect(scaleX - 5, scaleYStart - 5, 30, scaleHeight + 10, ST77XX_WHITE);
      tft.setTextSize(1);
      for (int i = 1; i <= 14; i++) {
          int y_pos = map(i, 0, 14, scaleHeight, 0);
          y_pos = scaleYStart + y_pos;
          uint16_t color = colorPH(i);
          tft.setTextColor(color);
          tft.setCursor(scaleX + 30, y_pos - 4);
          tft.printf("%2d", i);
          tft.drawLine(scaleX, y_pos, scaleX + 20, y_pos, color);
      }
      int yPHIndicator = map(phCalculado, 0, 14, scaleHeight, 0);
      yPHIndicator = scaleYStart + yPHIndicator;
      tft.fillCircle(scaleX + 10, yPHIndicator, 3, ST77XX_RED);
      tft.setTextColor(ST77XX_WHITE); tft.setCursor(tft.width() / 2 - (strlen("Regresar (SW)")*6)/2, tft.height() - 15); tft.print("Regresar (SW)");
      break;
    }
    case STATE_RUNOFF_SETTINGS: {
      tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2);
      tft.setCursor(20, 40); tft.print("Control de Runoff");
      drawSubMenuOption("ZONA 1", selectedSubMenuOption == 0, runoffZona1, 90);
      drawSubMenuOption("ZONA 2", selectedSubMenuOption == 1, runoffZona2, 120);
      drawSubMenuOption("AMBAS", selectedSubMenuOption == 2, runoffZona1 && runoffZona2, 150);
      tft.setTextSize(1);
      tft.setCursor(40, tft.height() - 20); tft.print("OK: Cambiar | SW: Salir");
      break;
    }
    case STATE_MANUAL_WATER_MENU: {
      tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2);
      tft.setCursor(20, 40); tft.print("Seleccionar Zona");
      drawSubMenuOption("ZONA 1", selectedSubMenuOption == 0, false, 90);
      drawSubMenuOption("ZONA 2", selectedSubMenuOption == 1, false, 120);
      tft.setTextSize(1);
      tft.setCursor(40, tft.height() - 20); tft.print("OK: Iniciar | SW: Salir");
      break;
    }
    case STATE_THRESHOLD_SETTINGS: {
      tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2);
      const char* title = "AJUSTAR UMBRALES";
      tft.setCursor((tft.width() - strlen(title) * 12) / 2, 20);
      tft.print(title);
      drawThresholdOption("Activar en:", humedadInicio, selectedThresholdOption == 0, (selectedThresholdOption == 0 && isEditingThreshold), 80);
      drawThresholdOption("Apagar en:", humedadFinal, selectedThresholdOption == 1, (selectedThresholdOption == 1 && isEditingThreshold), 120);
      tft.setTextColor(ST77XX_WHITE); tft.setTextSize(1);
      const char* instructions;
      if (isEditingThreshold) {
        instructions = "OK: Confirmar | Y: +/- | SW: Guardar";
      } else {
        instructions = "Y: Navegar | OK: Editar | SW: Guardar y Salir";
      }
      int textWidth = strlen(instructions) * 6;
      tft.setCursor((tft.width() - textWidth) / 2, tft.height() - 20);
      tft.print(instructions);
      break;
    }
    case STATE_MAIN_DISPLAY:
    case STATE_MENU_NAVIGATE: {
      if (mostrarHora) {
        DateTime now = rtc.now();
        char horaTexto[6]; snprintf(horaTexto, sizeof(horaTexto), "%02d:%02d", now.hour(), now.minute());
        tft.setTextColor(ST77XX_WHITE); tft.setTextSize(1);
        int textoAnchoHora = strlen(horaTexto) * 6; int xHora = (tft.width() - textoAnchoHora) / 2;
        tft.setCursor(xHora, currentY); tft.print(horaTexto);
        currentY += 10;
      }

      if (fertiriegoActivo) {
        tft.setTextColor(ST77XX_MAGENTA); tft.setTextSize(1);
        const char* fertiText = "FERTIRRIEGO ACTIVO";
        tft.setCursor((tft.width() - strlen(fertiText)*6)/2, currentY);
        tft.print(fertiText);
        currentY += 10;
      }

      if (runoffZona1 || runoffZona2) {
        tft.setTextColor(ST77XX_YELLOW); tft.setTextSize(1);
        char runoffText[20];
        if (runoffZona1 && runoffZona2) sprintf(runoffText, "RUNOFF AMBAS ACTIVO");
        else if (runoffZona1) sprintf(runoffText, "RUNOFF Z1 ACTIVO");
        else sprintf(runoffText, "RUNOFF Z2 ACTIVO");
        tft.setCursor((tft.width() - strlen(runoffText)*6)/2, currentY); tft.print(runoffText);
        currentY += 10;
      }
      int graficoY = currentY + 5;
      tft.setTextColor(ST77XX_WHITE); tft.setTextSize(1); tft.setCursor(10, graficoY); tft.print("% humedad");
      int grafX = 30, grafY = graficoY + 10, grafWidth = 100, grafHeight = 70;
      tft.drawRect(grafX, grafY, grafWidth, grafHeight, ST77XX_WHITE);
      int barWidth = 15;
      int barra1Altura = map(humedad1, 0, 100, 0, grafHeight);
      int barra2Altura = map(humedad2, 0, 100, 0, grafHeight);
      int barraZB1Altura = map(humedadZonaBaja1, 0, 100, 0, grafHeight);
      int barraZB2Altura = map(humedadZonaBaja2, 0, 100, 0, grafHeight);
      int barra1X = grafX + (grafWidth * 1 / 8) - (barWidth / 2);
      int barra2X = grafX + (grafWidth * 3 / 8) - (barWidth / 2);
      int barraZB1X = grafX + (grafWidth * 5 / 8) - (barWidth / 2);
      int barraZB2X = grafX + (grafWidth * 7 / 8) - (barWidth / 2);
      tft.fillRect(barra1X, grafY + grafHeight - barra1Altura, barWidth, barra1Altura, ST77XX_BLUE);
      tft.fillRect(barra2X, grafY + grafHeight - barra2Altura, barWidth, barra2Altura, ST77XX_GREEN);
      tft.fillRect(barraZB1X, grafY + grafHeight - barraZB1Altura, barWidth, barraZB1Altura, ST77XX_ORANGE);
      tft.fillRect(barraZB2X, grafY + grafHeight - barraZB2Altura, barWidth, barraZB2Altura, ST77XX_YELLOW);
      int zonaTextStartX = grafX + grafWidth + min_gap_between_zones;
      int zonaTextCurrentY = graficoY;
      tft.setTextColor(ST77XX_BLUE); tft.setCursor(zonaTextStartX, zonaTextCurrentY); tft.printf("Z1: %d%% %s", humedad1, regando1 ? "R" : "");
      zonaTextCurrentY += 12;
      tft.setTextColor(ST77XX_GREEN); tft.setCursor(zonaTextStartX, zonaTextCurrentY); tft.printf("Z2: %d%% %s", humedad2, regando2 ? "R" : "");
      zonaTextCurrentY += 12;
      tft.setTextColor(ST77XX_ORANGE); tft.setCursor(zonaTextStartX, zonaTextCurrentY); tft.printf("ZB1: %d%%", humedadZonaBaja1);
      zonaTextCurrentY += 12;
      tft.setTextColor(ST77XX_YELLOW); tft.setCursor(zonaTextStartX, zonaTextCurrentY); tft.printf("ZB2: %d%%", humedadZonaBaja2);
      if (currentMenuState == STATE_MENU_NAVIGATE) {
        int menuY = max(zonaTextCurrentY, grafY + grafHeight) + 15;
        drawMainMenuOption("RIEGO MANUAL", selectedMainMenuOption == MENU_RIEGO_MANUAL, false, menuY);
        drawMainMenuOption("FERTIRRIEGO",  selectedMainMenuOption == MENU_FERTIRRIEGO, fertiriegoActivo, menuY + 20);
        drawMainMenuOption("RUNOFF",       selectedMainMenuOption == MENU_RUNOFF, runoffZona1 || runoffZona2, menuY + 40);
        drawMainMenuOption("HISTORIA",     selectedMainMenuOption == MENU_HISTORIA, false, menuY + 60);
        drawMainMenuOption("pH",           selectedMainMenuOption == MENU_PH, false, menuY + 80);
        drawMainMenuOption("AJUSTES",      selectedMainMenuOption == MENU_AJUSTES, false, menuY + 100);
      }
      break;
    }
  }
}

void imprimirEstado() {
  DateTime now = rtc.now();
  char runoffStatus[10];
  if(runoffZona1 && runoffZona2) strcpy(runoffStatus, "AMBAS");
  else if(runoffZona1) strcpy(runoffStatus, "ZONA1");
  else if(runoffZona2) strcpy(runoffStatus, "ZONA2");
  else strcpy(runoffStatus, "INACTIVO");
  char fertiStatus[10];
  strcpy(fertiStatus, fertiriegoActivo ? "ACTIVO" : "INACTIVO");

  Serial.printf("[%02d/%02d %02d:%02d] Z1:%d%%|Z2:%d%%|ZB1:%d%%|ZB2:%d%%|pH:%.2f|RUNOFF:%s|FERTI:%s|UMBRAL:%d-%d\n",
                now.day(), now.month(), now.hour(), now.minute(),
                humedad1, humedad2, humedadZonaBaja1, humedadZonaBaja2, phCalculado, runoffStatus, fertiStatus,
                humedadInicio, humedadFinal);
}

void resetSleepTimer() {
    lastInteractionTime = millis();
    if (screenIsAsleep) {
        screenIsAsleep = false;
        tft.writeCommand(ST77XX_DISPON);
        screenNeedsRedraw = true;
    }
}

void manejarNavegacion() {
  static unsigned long lastBtnPress = 0;
  static int lastXState = 0;
  static int lastYState = 0;

  const int JOYSTICK_THRESHOLD_LOW = 800;
  const int JOYSTICK_THRESHOLD_HIGH = 3200;
  const int BTN_DEBOUNCE_TIME = 250;

  bool btnOK = (digitalRead(BTN_OK) == LOW);
  bool btnTAB = (digitalRead(BTN_TAB) == LOW);
  bool btnSW = (digitalRead(JOY_SW) == LOW);

  int joyX_raw = analogRead(JOY_X);
  int joyY_raw = analogRead(JOY_Y);

  int currentXState = (joyX_raw < JOYSTICK_THRESHOLD_LOW) ? -1 : (joyX_raw > JOYSTICK_THRESHOLD_HIGH ? 1 : 0);
  int currentYState = (joyY_raw < JOYSTICK_THRESHOLD_LOW) ? -1 : (joyY_raw > JOYSTICK_THRESHOLD_HIGH ? 1 : 0);

  if (btnOK || btnTAB || btnSW || currentXState != 0 || currentYState != 0) {
    resetSleepTimer();
  }

  if (screenIsAsleep) return;

  if (currentMenuState == STATE_MANUAL_WATER_ACTIVE && (btnOK || btnTAB)) {
      if(millis() - lastBtnPress > BTN_DEBOUNCE_TIME) {
          manualWateringActive = false;
          digitalWrite(relePin1, HIGH);
          digitalWrite(relePin2, HIGH);
          regando1 = false;
          regando2 = false;
          currentMenuState = STATE_MAIN_DISPLAY;
          screenNeedsRedraw = true;
          lastBtnPress = millis();
          return;
      }
  }

  if (millis() - lastBtnPress > BTN_DEBOUNCE_TIME) {
    if (currentMenuState == STATE_HISTORY_CALENDAR) {
        if (btnSW) {
            lastBtnPress = millis();
            screenNeedsRedraw = true;
            DateTime today = rtc.now();
            if (calendarDate.year() == today.year() && calendarDate.month() == today.month() && calendarDate.day() == today.day()) {
                sprintf(historyDateStr, "Hoy");
            } else {
                sprintf(historyDateStr, "%02d/%02d/%04d", calendarDate.day(), calendarDate.month(), calendarDate.year());
            }
            noHistoryDataForDay = !loadHistory(calendarDate);
            currentMenuState = STATE_HISTORY_DISPLAY;
        } else if (btnOK) {
            lastBtnPress = millis();
            screenNeedsRedraw = true;
            calendarDate = calendarDate + TimeSpan(1, 0, 0, 0);
        } else if (btnTAB) {
            lastBtnPress = millis();
            screenNeedsRedraw = true;
            calendarDate = calendarDate + TimeSpan(-1, 0, 0, 0);
        }
    }
    else {
        if (btnOK) {
          lastBtnPress = millis();
          screenNeedsRedraw = true;
          switch(currentMenuState) {
            case STATE_MAIN_DISPLAY: currentMenuState = STATE_MENU_NAVIGATE; selectedMainMenuOption = 1; break;
            case STATE_MENU_NAVIGATE:
              if (selectedMainMenuOption == MENU_RIEGO_MANUAL) { currentMenuState = STATE_MANUAL_WATER_MENU; selectedSubMenuOption = 0; }
              else if(selectedMainMenuOption == MENU_FERTIRRIEGO) {
                   fertiriegoActivo = !fertiriegoActivo;
                   if (!fertiriegoActivo) {
                       // Si se desactiva manualmente, para todo
                       estadoFertiriego = FERTIRIEGO_INACTIVO;
                       digitalWrite(relePin1, HIGH);
                       digitalWrite(relePin2, HIGH);
                       regando1 = false;
                       regando2 = false;
                   } else {
                       // Al activar, se pone en modo de espera para el primer ciclo
                       estadoFertiriego = FERTIRIEGO_ESPERA;
                       inicioCicloFertiriego = millis(); // Empieza el conteo de 40 min.
                       Serial.println("Modo Fertiriego Activado. Primer ciclo en 40 minutos.");
                   }
              }
              else if(selectedMainMenuOption == MENU_RUNOFF) { currentMenuState = STATE_RUNOFF_SETTINGS; selectedSubMenuOption = 0; }
              else if(selectedMainMenuOption == MENU_HISTORIA) { calendarDate = rtc.now(); currentMenuState = STATE_HISTORY_CALENDAR; }
              else if(selectedMainMenuOption == MENU_PH) currentMenuState = STATE_PH_DISPLAY;
              else if(selectedMainMenuOption == MENU_AJUSTES) currentMenuState = STATE_THRESHOLD_SETTINGS;
              break;
            case STATE_THRESHOLD_SETTINGS: isEditingThreshold = !isEditingThreshold; break;
            case STATE_RUNOFF_SETTINGS: {
                if(selectedSubMenuOption == 0) runoffZona1 = !runoffZona1;
                else if(selectedSubMenuOption == 1) runoffZona2 = !runoffZona2;
                else if(selectedSubMenuOption == 2) {
                    bool newState = !(runoffZona1 && runoffZona2);
                    runoffZona1 = newState; runoffZona2 = newState;
                }
                if (!manualWateringActive && !fertiriegoActivo) {
                    digitalWrite(relePin1, runoffZona1 ? LOW : HIGH); regando1 = runoffZona1;
                    digitalWrite(relePin2, runoffZona2 ? LOW : HIGH); regando2 = runoffZona2;
                }
                break;
            }
            case STATE_MANUAL_WATER_MENU: {
                manualWateringActive = true;
                manualWateringZone = selectedSubMenuOption + 1;
                manualWateringEndTime = millis() + MANUAL_WATERING_DURATION;
                if(manualWateringZone == 1) { digitalWrite(relePin1, LOW); regando1 = true; }
                else { digitalWrite(relePin2, LOW); regando2 = true; }
                currentMenuState = STATE_MANUAL_WATER_ACTIVE;
                break;
            }
            default: break;
          }
        }
        else if (btnSW) {
          lastBtnPress = millis();
          screenNeedsRedraw = true;
          if (currentMenuState != STATE_MAIN_DISPLAY) {
            if (currentMenuState == STATE_THRESHOLD_SETTINGS) {
                if (isEditingThreshold) {
                    isEditingThreshold = false;
                } else {
                    if (humedadInicio >= humedadFinal) humedadInicio = max(0, humedadFinal - 1);
                    EEPROM.write(ADDR_HUMEDAD_INICIO, humedadInicio);
                    EEPROM.write(ADDR_HUMEDAD_FINAL, humedadFinal);
                    EEPROM.commit();
                    currentMenuState = STATE_MAIN_DISPLAY;
                }
            } else {
              currentMenuState = STATE_MAIN_DISPLAY;
            }
          }
        }
        else if (btnTAB) {
          lastBtnPress = millis();
          screenNeedsRedraw = true;
          if (currentMenuState != STATE_MAIN_DISPLAY) {
            currentMenuState = STATE_MAIN_DISPLAY;
            isEditingThreshold = false;
          }
        }
    }
  }

  if (currentYState != lastYState && currentYState != 0) {
      screenNeedsRedraw = true;
      lastYState = currentYState;
      if (currentMenuState == STATE_MENU_NAVIGATE) {
        selectedMainMenuOption += (currentYState == 1 ? 1 : -1);
        if (selectedMainMenuOption < 1) selectedMainMenuOption = MENU_TOTAL;
        if (selectedMainMenuOption > MENU_TOTAL) selectedMainMenuOption = 1;
      } else if (currentMenuState == STATE_THRESHOLD_SETTINGS) {
        if (isEditingThreshold) {
            int valueChange = (currentYState == 1 ? 1 : -1);
            if (selectedThresholdOption == 0) humedadInicio = constrain(humedadInicio + valueChange, 0, 100);
            else humedadFinal = constrain(humedadFinal + valueChange, 0, 100);
        } else {
            selectedThresholdOption = 1 - selectedThresholdOption;
        }
      } else if (currentMenuState == STATE_RUNOFF_SETTINGS) {
        selectedSubMenuOption += (currentYState == 1 ? 1 : -1);
        if (selectedSubMenuOption < 0) selectedSubMenuOption = 2;
        if (selectedSubMenuOption > 2) selectedSubMenuOption = 0;
      } else if (currentMenuState == STATE_MANUAL_WATER_MENU) {
        selectedSubMenuOption = 1 - selectedSubMenuOption;
      } else if (currentMenuState == STATE_HISTORY_CALENDAR) {
          int daysToMove = (currentYState == 1 ? 7 : -7);
          calendarDate = calendarDate + TimeSpan(daysToMove, 0, 0, 0);
      }
  } else if (currentYState == 0) {
      lastYState = 0;
  }
}

void mostrarPantallaInicio() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN); tft.setTextSize(2);
  String titulo1 = "Sistema de"; String titulo2 = "Riego";
  tft.setCursor((240 - titulo1.length() * 12) / 2, 80); tft.print(titulo1);
  tft.setCursor((240 - titulo2.length() * 12) / 2, 100); tft.print(titulo2);
  tft.setTextColor(ST77XX_WHITE); tft.setTextSize(1);
  String instr1 = "Presiona cualquier boton"; String instr2 = "para continuar...";
  tft.setCursor((240 - instr1.length() * 6) / 2, 140); tft.print(instr1);
  tft.setCursor((240 - instr2.length() * 6) / 2, 155); tft.print(instr2);
}

void esperarBoton() {
  while (true) {
    if (digitalRead(BTN_OK) == LOW || digitalRead(BTN_TAB) == LOW || digitalRead(JOY_SW) == LOW ||
        analogRead(JOY_X) < 500 || analogRead(JOY_X) > 3500 ||
        analogRead(JOY_Y) < 500 || analogRead(JOY_Y) > 3500) {
      delay(500);
      return;
    }
  }
}

void manejarClienteWiFi() {
  WiFiClient client = server.available();
  if (!client) return;
  String req = client.readStringUntil('\r'); client.flush();
  if (!runoffZona1 && !runoffZona2 && !manualWateringActive && !fertiriegoActivo) {
    if (req.indexOf("GET /toggle1") >= 0) {
      regando1 = !regando1; digitalWrite(relePin1, regando1 ? LOW : HIGH);
    }
    if (req.indexOf("GET /toggle2") >= 0) {
      regando2 = !regando2; digitalWrite(relePin2, regando2 ? LOW : HIGH);
    }
  }
  client.println("HTTP/1.1 200 OK\nContent-Type: text/html\nConnection: close\n");
  client.println("<!DOCTYPE html><html><head><title>Sistema de Riego</title></head><body>");
  client.println("<h1>Sistema de Riego</h1>");
  client.println("</body></html>");
  client.stop();
}

// ==================================================================
// ======== FUNCIÓN DE FERTIRRIEGO MODIFICADA =======================
// ==================================================================
void manejarFertiriego() {
    if (!fertiriegoActivo) {
        return;
    }

    unsigned long ahora = millis();

    switch (estadoFertiriego) {
        case FERTIRIEGO_ESPERA:
            // Espera a que pasen los 40 minutos para iniciar el ciclo
            if (ahora - inicioCicloFertiriego >= FERTIRIEGO_INTERVALO) {
                Serial.println("FERTIRRIEGO: Iniciando ciclo. Activando Bomba 1 por 5s.");
                digitalWrite(relePin1, LOW); // Activa bomba 1
                regando1 = true;
                digitalWrite(relePin2, HIGH); // Asegura que bomba 2 está apagada
                regando2 = false;
                cambioBombaFertirriego = ahora; // Guarda el tiempo para el cambio
                estadoFertiriego = FERTIRIEGO_BOMBA1;
                screenNeedsRedraw = true;
            }
            break;

        case FERTIRIEGO_BOMBA1:
            // Espera 5 segundos para cambiar a la bomba 2
            if (ahora - cambioBombaFertirriego >= FERTIRIEGO_DURACION_BOMBA) {
                Serial.println("FERTIRRIEGO: Activando Bomba 2 por 5s.");
                digitalWrite(relePin1, HIGH); // Apaga bomba 1
                regando1 = false;
                digitalWrite(relePin2, LOW);  // Activa bomba 2
                regando2 = true;
                cambioBombaFertirriego = ahora; // Guarda el tiempo para finalizar
                estadoFertiriego = FERTIRIEGO_BOMBA2;
                screenNeedsRedraw = true;
            }
            break;

        case FERTIRIEGO_BOMBA2:
            // Espera 5 segundos para finalizar el ciclo
            if (ahora - cambioBombaFertirriego >= FERTIRIEGO_DURACION_BOMBA) {
                Serial.println("FERTIRRIEGO: Ciclo finalizado. Esperando 40 minutos.");
                digitalWrite(relePin2, HIGH); // Apaga bomba 2
                regando2 = false;
                inicioCicloFertiriego = ahora; // Reinicia el temporizador de 40 minutos
                estadoFertiriego = FERTIRIEGO_ESPERA;
                screenNeedsRedraw = true;
            }
            break;

        case FERTIRIEGO_INACTIVO:
            // No hacer nada
            break;
    }
}


void setup() {
  Serial.begin(115200);
  Serial.println("\nIniciando Emisor (Sensores) v4.9.4...");

  EEPROM.begin(16);
  humedadInicio = EEPROM.read(ADDR_HUMEDAD_INICIO);
  humedadFinal = EEPROM.read(ADDR_HUMEDAD_FINAL);
  if (humedadInicio > 100 || humedadInicio < 0) { humedadInicio = 25; }
  if (humedadFinal > 100 || humedadFinal < 0 || humedadFinal <= humedadInicio) { humedadFinal = 60; }

  tft.init(240, 240, SPI_MODE0); tft.setRotation(0);
  mostrarPantallaInicio();

  if(!SPIFFS.begin(true)){ Serial.println("Error montando SPIFFS"); while(1); }
  if (!SPIFFS.exists("/hist")) { SPIFFS.mkdir("/hist"); }

  pinMode(BTN_OK, INPUT_PULLUP); pinMode(BTN_TAB, INPUT_PULLUP); pinMode(JOY_X, INPUT); pinMode(JOY_Y, INPUT); pinMode(JOY_SW, INPUT_PULLUP);
  esperarBoton();

  pinMode(sensorPin1, INPUT); pinMode(sensorPin2, INPUT); pinMode(SENSOR_ZONA_BAJA1, INPUT); pinMode(SENSOR_ZONA_BAJA2, INPUT);
  pinMode(relePin1, OUTPUT); pinMode(relePin2, OUTPUT); digitalWrite(relePin1, HIGH); digitalWrite(relePin2, HIGH);
  pinMode(ledWifi, OUTPUT); pinMode(SQW_PIN, INPUT); pinMode(PH_DIGITAL_PIN, INPUT_PULLUP);
  Wire.begin(I2C_SDA, I2C_SCL); rtc.begin();

  Serial.println("Conectando a WiFi para NTP...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500); Serial.print("."); attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado a WiFi para NTP! Sincronizando RTC...");
    digitalWrite(ledWifi, HIGH);
    timeClient.begin();
    if (timeClient.forceUpdate()) {
      rtc.adjust(DateTime(timeClient.getEpochTime()));
      Serial.println("OK: RTC sincronizado.");
    }
    timeClient.end();
  } else {
    Serial.println("\nFallo al conectar a WiFi para NTP.");
    digitalWrite(ledWifi, LOW);
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  Serial.println("Configurando ESP-NOW y SoftAP...");
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("FATAL: Error inicializando ESP-NOW");
    return;
  }

  // Aquí es donde se registran las funciones de callback
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  WiFi.softAP(ap_ssid, NULL, 1);
  server.begin();

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("FATAL: Fallo al agregar el peer.");
    return;
  } else {
    Serial.println("OK: Peer de ESP-NOW agregado.");
  }

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(2025, 7, 12, 18, 0, 0));
  }

  DateTime now = rtc.now();
  currentDay = now.day(); currentMonth = now.month();
  readSensorsAndControlRelays();
  lastInteractionTime = millis();
}

void loop() {
  unsigned long ahora = millis();

  manejarFertiriego();

  if (!screenIsAsleep && (ahora - lastInteractionTime > SCREEN_SLEEP_TIMEOUT)) {
      screenIsAsleep = true;
      tft.writeCommand(ST77XX_DISPOFF);
      screenNeedsRedraw = false;
  }

  manejarNavegacion();

  if (manualWateringActive && ahora >= manualWateringEndTime) {
      manualWateringActive = false;
      digitalWrite(relePin1, HIGH);
      digitalWrite(relePin2, HIGH);
      regando1 = false;
      regando2 = false;
      currentMenuState = STATE_MAIN_DISPLAY;
      screenNeedsRedraw = true;
      resetSleepTimer();
  }

  if (!manualWateringActive && (ahora - ultimoChequeo >= intervaloChequeo)) {
    readSensorsAndControlRelays();
    ultimoChequeo = ahora;
    if (currentMenuState == STATE_MAIN_DISPLAY) {
        screenNeedsRedraw = true;
    }
  }

  if (currentMenuState == STATE_PH_DISPLAY && ahora - lastPhScreenUpdateTime >= phScreenUpdateInterval) {
    readSensorsAndControlRelays();
    screenNeedsRedraw = true;
    lastPhScreenUpdateTime = ahora;
  }

  if (ahora - ultimoPrint >= intervaloPrint) {
    imprimirEstado();
    enviarDatosPorEspNow();
    ultimoPrint = ahora;
  }

  manejarClienteWiFi();

  if (currentMenuState == STATE_MANUAL_WATER_ACTIVE) {
      screenNeedsRedraw = true;
  }

  if (screenNeedsRedraw && !screenIsAsleep) {
    drawCurrentScreenState();
    screenNeedsRedraw = false;
  }
}