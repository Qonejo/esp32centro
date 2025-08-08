#include <SPIFFS.h>
#include <Arduino.h> // Necesario para funciones como analogRead, digitalWrite, etc.
#include "sensores.h"
#include "config.h"

// Implementación de las funciones de sensores y actuadores.
// Este código fue cortado del archivo .ino principal.

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
