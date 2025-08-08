#ifndef PANTALLA_H
#define PANTALLA_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <RTClib.h>

// Para evitar incluir todo el .ino y crear dependencias circulares,
// declaramos los enums y objetos globales como 'extern'.
// Esto le dice al compilador "estas variables existen en otro lugar".

// Forward declaration of enums
enum MenuState;
enum FertiriegoState;

// Objetos globales definidos en el archivo .ino principal
extern Adafruit_ST7789 tft;
extern RTC_DS3231 rtc;

// Variables de estado globales usadas por las funciones de la pantalla
extern MenuState currentMenuState;
extern bool screenIsAsleep;
extern int manualWateringZone;
extern unsigned long manualWateringEndTime;
extern char historyDateStr[11];
extern bool noHistoryDataForDay;
extern int hourlyHumidity1[24], hourlyHumidity2[24], hourlyHumidityZonaBaja1[24], hourlyHumidityZonaBaja2[24], hourlyReadingsCount[24];
extern float phCalculado;
extern bool runoffZona1, runoffZona2;
extern int selectedSubMenuOption;
extern int humedadInicio, humedadFinal;
extern int selectedThresholdOption;
extern bool isEditingThreshold;
extern bool mostrarHora;
extern bool fertiriegoActivo;
extern int humedad1, humedad2, humedadZonaBaja1, humedadZonaBaja2;
extern bool regando1, regando2;
extern int selectedMainMenuOption;
extern DateTime calendarDate;

// --- Prototipos de Funciones ---
// Estas son las funciones que moveremos a pantalla.cpp
void drawMainMenuOption(const char* label, bool isSelected, bool isActiveState, int yPos);
void drawSubMenuOption(const char* label, bool isSelected, bool isActive, int yPos);
void drawThresholdOption(const char* label, int value, bool isSelected, bool isEditing, int yPos);
uint16_t colorPH(float ph);
void drawCalendar();
void drawCurrentScreenState();
void mostrarPantallaInicio();

#endif // PANTALLA_H
