#ifndef CONFIG_H
#define CONFIG_H

// ==========================================================================================
// ===== ARCHIVO DE CONFIGURACIÓN PRINCIPAL =================================================
// ==========================================================================================
// Este archivo centraliza todas las constantes, pines y configuraciones del proyecto.

// ==================== CONFIGURACIÓN DE COMUNICACIÓN ====================
// !! IMPORTANTE: ESTA ES LA DIRECCIÓN MAC DEL RECEPTOR (T-DISPLAY) !!
uint8_t broadcastAddress[] = {0x8C, 0x4F, 0x00, 0xD3, 0xAE, 0xB4};

// ==================== CONFIGURACIÓN DE RED Y HORA ====================
#define UTC_OFFSET_SECONDS -21600 // UTC-6 (HORARIO ESTÁNDAR DE MÉXICO CENTRAL)
const char* ap_ssid = "ESP32_Riego";
const char* WIFI_SSID = "IZZI-D1A8";
const char* WIFI_PASSWORD = "HJ39R2YLMJ9H";

// ==================== PINES DE HARDWARE ====================
// --- Sensores de Humedad ---
const int sensorPin1 = 35;
const int sensorPin2 = 32;
const int SENSOR_ZONA_BAJA1 = 36;
const int SENSOR_ZONA_BAJA2 = 39;

// --- Relés (Bombas) ---
const int relePin1   = 27;
const int relePin2   = 14;

// --- LED de Estado ---
const int ledWifi    = 2;

// --- Pantalla TFT (SPI) ---
#define TFT_CS    15
#define TFT_RST   4
#define TFT_DC    5
#define TFT_SCLK  18
#define TFT_MOSI  23

// --- RTC (I2C) ---
#define I2C_SDA   22
#define I2C_SCL   21
#define SQW_PIN   19

// --- Sensor de pH ---
#define PH_ANALOG_PIN 34
#define PH_DIGITAL_PIN 16
#define PH_TEMP_PIN 17

// --- Controles (Joystick y Botones) ---
#define JOY_X     17
#define JOY_Y     33
#define JOY_SW    26
#define BTN_TAB   12
#define BTN_OK    13

// ==================== VALORES FIJOS DE CALIBRACIÓN ====================
// Estos valores deben ser ajustados para tus sensores específicos.
// 'Dry' es el valor leído con el sensor al aire.
// 'Wet' es el valor leído con el sensor sumergido en agua.
const int humedad1DryRaw = 3260;
const int humedad1WetRaw = 1200;
const int humedad2DryRaw = 3270;
const int humedad2WetRaw = 1100;
const int humedadZonaBaja1DryRaw = 3219;
const int humedadZonaBaja1WetRaw = 1099;
const int humedadZonaBaja2DryRaw = 3219;
const int humedadZonaBaja2WetRaw = 1107;

// ==================== PARÁMETROS DE CONTROL ====================
// --- Umbrales de Riego ---
#define ADDR_HUMEDAD_INICIO 0 // Dirección en EEPROM
#define ADDR_HUMEDAD_FINAL  1 // Dirección en EEPROM

// --- Riego Manual ---
const unsigned long MANUAL_WATERING_DURATION = 20000; // 20 segundos

// --- Fertirriego ---
const unsigned long FERTIRIEGO_INTERVALO = 40 * 60 * 1000UL; // 40 minutos
const unsigned long FERTIRIEGO_DURACION_BOMBA = 5 * 1000UL;  // 5 segundos por bomba

// --- Reposo de Pantalla ---
const unsigned long SCREEN_SLEEP_TIMEOUT = 60000; // 1 minuto

// --- pH ---
const float phOffset = 0.0; // Offset para calibración de pH

// --- Intervalos de Chequeo ---
const unsigned long intervaloChequeo = 3000; // 3 segundos
const unsigned long intervaloPrint = 1000;   // 1 segundo
const unsigned long phScreenUpdateInterval = 1000; // 1 segundo

#endif // CONFIG_H
