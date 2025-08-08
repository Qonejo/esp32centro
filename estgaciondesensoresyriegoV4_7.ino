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

#include "config.h"
#include "pantalla.h"
#include "sensores.h"
#include "ui.h"

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

// ==================== OBJETOS GLOBALES ====================
WiFiServer server(80);
WiFiUDP ntpUdp;
NTPClient timeClient(ntpUdp, "pool.ntp.org", UTC_OFFSET_SECONDS, 60000);
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
RTC_DS3231 rtc;


// ==================== VARIABLES DE ESTADO Y CONTROL ====================
int humedadInicio;
int humedadFinal;
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

// ==================== VARIABLES PARA FERTIRRIEGO ====================
bool fertiriegoActivo = false;
unsigned long inicioCicloFertiriego = 0;
unsigned long cambioBombaFertirriego = 0;
enum FertiriegoState { FERTIRIEGO_INACTIVO, FERTIRIEGO_BOMBA1, FERTIRIEGO_BOMBA2, FERTIRIEGO_ESPERA };
FertiriegoState estadoFertiriego = FERTIRIEGO_INACTIVO;

// --- REPOSO ---
unsigned long lastInteractionTime = 0;
bool screenIsAsleep = false;

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
float phCalculado = 0.0;
unsigned long ultimoChequeo = 0; unsigned long ultimoPrint   = 0;
unsigned long lastPhScreenUpdateTime = 0;
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