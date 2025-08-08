#include "pantalla.h"
#include "config.h" // Incluimos por si se usan constantes de configuración

// Implementación de las funciones de dibujo.
// Este código fue cortado del archivo .ino principal.

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
