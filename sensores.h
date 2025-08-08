#ifndef SENSORES_H
#define SENSORES_H

#include <RTClib.h>

// El compilador necesita saber que estos objetos y variables existen en otro archivo.
// Los declaramos como 'extern'.

// Objetos globales
extern RTC_DS3231 rtc;

// Variables de estado globales
extern int humedad1, humedad2, humedadZonaBaja1, humedadZonaBaja2;
extern bool regando1, regando2;
extern float phCalculado;
extern int currentDay, currentMonth;
extern int hourlyHumidity1[24], hourlyHumidity2[24], hourlyHumidityZonaBaja1[24], hourlyHumidityZonaBaja2[24], hourlyReadingsCount[24];
extern bool manualWateringActive, runoffZona1, runoffZona2, fertiriegoActivo;
extern int humedadInicio, humedadFinal;
extern int phValorAnalogico;
extern bool phAlto;
extern float phVoltaje;

// Prototipos de las funciones que implementaremos en sensores.cpp
void readSensorsAndControlRelays();
void saveHistory();
bool loadHistory(DateTime dateToLoad);

#endif // SENSORES_H
