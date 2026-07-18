import sys

with open('led_display.h', 'r', encoding='utf-8') as f:
    lines = f.readlines()

new_content = ''
for i, line in enumerate(lines):
    if 'inline bool isTicketCode' in line:
        break
    new_content += line

new_code = '''// Helper function to check if the string is a ticket code (like BH-001, KH-0002, A-12, etc.)
inline bool isTicketCode(const char* text) {
  int len = strlen(text);
  if (len == 0 || len > 12) return false;
  
  bool hasDigits = false;
  for (int i = 0; text[i] != '\\0'; i++) {
    if (isdigit((unsigned char)text[i])) {
      hasDigits = true;
      break;
    }
  }
  return hasDigits;
}

// Process received display command
inline void processMessage(const char* command) {
  if (strcmp(command, "clear") == 0 || strcmp(command, "clear_display") == 0 || strlen(command) == 0) {
    strcpy(textToDisplay, "");
    if (dma_display) dma_display->clearScreen();
    needRedraw = true;
    return;
  }

  char colorName[16] = {0};
  char text[250] = {0}; 
  
  const char* space = strchr(command, ' ');
  
  if (space == NULL) {
    strncpy(text, command, 249);
  } else {
    int colorNameLen = space - command;
    if (colorNameLen < 15) {
      strncpy(colorName, command, colorNameLen);
      colorName[colorNameLen] = '\\0';
    }
    strncpy(text, space + 1, 249);
  }
  
  if (strlen(colorName) > 0) {
    bool colorFound = false;
    for (int i = 0; i < NUM_COLORS; i++) {
      if (strcmp(colorName, colorDict[i].name) == 0) {
        textColor = colorDict[i].color;
        colorFound = true;
        break;
      }
    }
    if (!colorFound) {
      strncpy(text, command, 249);
      textColor = 0xFFFF;
    }
  } else {
    textColor = 0xFFFF; // Default white
  }
  
  if (strlen(text) > 0) {
    strcpy(textToDisplay, text);
    needRedraw = true;   
  }
}

// Display layout calculation
inline void updateDisplay() {
  if (!dma_display || strlen(textToDisplay) == 0) return;
  dma_display->clearScreen();
  
  int16_t yPos = PANEL_RES_Y / 2 + 6;
  int calculatedW = 0;
  
  // Get font for ticket code
  SelectedFontType selectedFont = getFontForTicketCode(textToDisplay, &yPos, &calculatedW);
  int tWidth = calculatedW;
  
  int16_t xPos = (PANEL_RES_X - tWidth) / 2; 
  if (xPos < 0) xPos = 0; // Prevent drawing outside left bound if slightly too large
  
  dma_display->setTextColor(textColor);
  if (selectedFont == FONT_U8G2_LARGE_3) {
    u8g2Fonts.setFont(LARGE_TICKET_FONT_3);
    u8g2Fonts.setForegroundColor(textColor);
    u8g2Fonts.setCursor(xPos, yPos);
    u8g2Fonts.print(textToDisplay);
  } else if (selectedFont == FONT_U8G2_LARGE_4) {
    u8g2Fonts.setFont(LARGE_TICKET_FONT_4);
    u8g2Fonts.setForegroundColor(textColor);
    u8g2Fonts.setCursor(xPos, yPos);
    u8g2Fonts.print(textToDisplay);
  } else if (selectedFont == FONT_SANS_9_BOLD) {
    dma_display->setFont(&FreeSansBold9pt7b);
    dma_display->setCursor(xPos, yPos);
    dma_display->print(textToDisplay);
  } else {
    u8g2Fonts.setFont(VIETNAMESE_FONT);
    u8g2Fonts.setForegroundColor(textColor);
    u8g2Fonts.setCursor(xPos, yPos);
    u8g2Fonts.print(textToDisplay);
  }
}

// Task loop for LED scanning
inline void led_display_task(void *pvParameters) {
  while (1) {
    if (needRedraw) {
      updateDisplay();
      needRedraw = false;
    }
    vTaskDelay(pdMS_TO_TICKS(50)); 
  }
}

// Task to read commands from Serial/UART
inline void serial_command_task(void *pvParameters) {
  char buffer[256];
  int idx = 0;
  while (1) {
    int c = getchar();
    if (c != EOF && c != 0xFF && c != 0) {
      if (c == '\\n' || c == '\\r') {
        if (idx > 0) {
          buffer[idx] = '\\0';
          ESP_LOGI("Serial", "Serial Command: %s", buffer);
          processMessage(buffer);
          idx = 0;
        }
      } else if (idx < sizeof(buffer) - 1) {
        buffer[idx++] = (char)c;
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}

// Main initialization
inline void setup_led_display() {
  HUB75_I2S_CFG::i2s_pins pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
  
  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,   
    PANEL_RES_Y,   
    PANEL_CHAIN,   
    pins           
  );
  
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  
  if (!dma_display->begin()) {
    ESP_LOGE("LED_MATRIX", "Failed to allocate DMA memory for LED matrix");
    return;
  }

  dma_display->setBrightness8(64); 
  dma_display->clearScreen();

  u8g2Fonts.begin(*dma_display);
  u8g2Fonts.setFontMode(1); 
  u8g2Fonts.setFontDirection(0);

  xTaskCreate(led_display_task, "led_display", 4096, NULL, 5, NULL);
  xTaskCreate(serial_command_task, "serial_cmd", 4096, NULL, 4, NULL);
}

#endif // LED_DISPLAY_H
'''
new_content += new_code
with open('led_display.h', 'w', encoding='utf-8') as f:
    f.write(new_content)
print('Rewrote led_display.h successfully')
