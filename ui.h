#ifndef UI_H
#define UI_H

#include <RTClib.h>
#include <Adafruit_ST7789.h>

// Declaraciones anticipadas de enums para evitar dependencias circulares
enum MenuState;
enum FertiriegoState;

// Declaraciones externas de objetos y variables globales que se usan en este módulo
extern Adafruit_ST7789 tft;
extern unsigned long lastInteractionTime;
extern bool screenIsAsleep;
extern bool screenNeedsRedraw;
extern bool manualWateringActive;
extern bool regando1, regando2;
extern MenuState currentMenuState;
extern DateTime calendarDate;
extern char historyDateStr[11];
extern bool noHistoryDataForDay;
extern int selectedMainMenuOption;
extern bool fertiriegoActivo;
extern FertiriegoState estadoFertiriego;
extern unsigned long inicioCicloFertiriego;
extern int selectedSubMenuOption;
extern bool runoffZona1, runoffZona2;
extern bool isEditingThreshold;
extern int humedadInicio, humedadFinal;
extern int selectedThresholdOption;
extern int manualWateringZone;
extern unsigned long manualWateringEndTime;

// Prototipos de funciones para la UI
void manejarNavegacion();
void resetSleepTimer();
void esperarBoton();

#endif // UI_H
