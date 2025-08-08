#include <Arduino.h>
#include "ui.h"
#include "config.h"
#include "sensores.h" // Se necesita para la función loadHistory()

// Implementación de las funciones de la interfaz de usuario (UI).
// Este código fue cortado del archivo .ino principal.

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
