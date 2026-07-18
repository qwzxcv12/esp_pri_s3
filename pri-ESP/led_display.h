#ifndef LED_DISPLAY_H
#define LED_DISPLAY_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Arduino.h"

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <Fonts/FreeSansBold12pt7b.h> 
#include <Fonts/FreeSansBold9pt7b.h>

// Define the connected PINs between P5 and ESP32
#define R1_PIN 25
#define G1_PIN 26
#define B1_PIN 27
#define R2_PIN 14
#define G2_PIN 12
#define B2_PIN 13
#define A_PIN 23
#define B_PIN 19
#define C_PIN 5
#define D_PIN 17
#define E_PIN -1  
#define LAT_PIN 4
#define OE_PIN 15
#define CLK_PIN 16

// Define P5 Panel configuration
#define PANEL_RES_X 64  
#define PANEL_RES_Y 32  
#define PANEL_CHAIN 1   

// Global pointers
inline MatrixPanel_I2S_DMA *dma_display = nullptr;
inline U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// Text display variables
inline char textToDisplay[250] = "Ready"; 
inline uint16_t textColor = 0xFFFF;    // Default white
inline bool needRedraw = true;
inline bool show_startup_messages = true;
inline char g_sta_ip[32] = "";
inline int blink_counter = 0;
inline bool is_blank_state = false;
extern bool g_wifi_connected;
extern bool g_mqtt_connected;

inline const uint8_t icon_wifi_16[32] = {
  0x00, 0x00, 0x0F, 0xF0, 0x3F, 0xFC, 0x78, 0x1E, 0xE0, 0x07, 0xC3, 0xC3, 0x0F, 0xF0, 0x1C, 0x38,
  0x30, 0x0C, 0x23, 0xC4, 0x07, 0xE0, 0x0C, 0x30, 0x00, 0x00, 0x03, 0xC0, 0x03, 0xC0, 0x01, 0x80
};
inline const uint8_t icon_mqtt_16[32] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x9F, 0xFE, 0x0F, 0xFC, 0xF7, 0xF9, 0xFB,
  0xF3, 0xCD, 0xE7, 0x86, 0xCF, 0x36, 0xDE, 0x7A, 0x9C, 0xFE, 0x3D, 0xFF, 0x39, 0xFF, 0x7B, 0xFF
};




// Color mapping
struct ColorMapping {
  const char* name;
  uint16_t color;
};

inline const ColorMapping colorDict[] = {
  {"do", 0xF800},      // Red
  {"xanh", 0x07E0},    // Green
  {"lam", 0x001F},     // Blue
  {"vang", 0xFFE0},    // Yellow
  {"tim", 0x780F},     // Purple
  {"cam", 0xFD20},     // Orange
  {"trang", 0xFFFF},   // White
  {"hong", 0xF81F}     // Pink
};

inline const int NUM_COLORS = sizeof(colorDict) / sizeof(colorDict[0]);


// Font selection
inline const uint8_t* VIETNAMESE_FONT = u8g2_font_unifont_t_vietnamese1;
inline const uint8_t* LARGE_TICKET_FONT_3 = u8g2_font_fub25_tr; // 25px for <=3 chars
inline const uint8_t* LARGE_TICKET_FONT_4 = u8g2_font_fub20_tr; // 20px for 4 chars

enum SelectedFontType {
  FONT_U8G2_LARGE_3,  // short codes (<=3 chars)
  FONT_U8G2_LARGE_4,  // short codes (4 chars)
  FONT_SANS_9_BOLD,   // FreeSansBold9pt7b for medium codes (5-6 chars)
  FONT_U8G2_VIETNAMESE // u8g2_font_unifont for long codes (>=7 chars)
};

inline SelectedFontType getFontForTicketCode(const char* text, int16_t* yOffset, int* calculatedWidth) {
  if (!dma_display) {
    *yOffset = PANEL_RES_Y / 2 + 6;
    *calculatedWidth = 0;
    return FONT_U8G2_VIETNAMESE;
  }
  
  int len = strlen(text);
  int16_t x1, y1;
  uint16_t w, h;
  
  // 1a. For length <= 3 (e.g. 124): Use large u8g2 font (FUB 25px)
  if (len <= 3) {
    u8g2Fonts.setFont(LARGE_TICKET_FONT_3);
    int width = u8g2Fonts.getUTF8Width(text);
    *yOffset = PANEL_RES_Y - 3; // baseline near bottom for 25px font on 32px panel
    *calculatedWidth = width;
    return FONT_U8G2_LARGE_3;
  }
  
  // 1b. For length == 4 (e.g. A-12): Use medium-large u8g2 font (FUB 20px)
  if (len == 4) {
    u8g2Fonts.setFont(LARGE_TICKET_FONT_4);
    int width = u8g2Fonts.getUTF8Width(text);
    *yOffset = PANEL_RES_Y - 5; // baseline near bottom for 20px font
    *calculatedWidth = width;
    return FONT_U8G2_LARGE_4;
  }
  
  // 2. For length 5-6 (e.g. BH-001, BH-005): Use FreeSansBold9pt7b
  if (len <= 6) {
    dma_display->setFont(&FreeSansBold9pt7b);
    dma_display->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    *yOffset = PANEL_RES_Y / 2 + 5; // yPos baseline for 9pt bold
    *calculatedWidth = (int)w;
    return FONT_SANS_9_BOLD;
  }
  
  // 3. For length >= 7 (e.g. BH-0001, KH-0002): Fallback to Vietnamese font (u8g2)
  u8g2Fonts.setFont(VIETNAMESE_FONT);
  int width = u8g2Fonts.getUTF8Width(text);
  *yOffset = PANEL_RES_Y / 2 + 6; // yPos baseline for Vietnamese font
  *calculatedWidth = width;
  return FONT_U8G2_VIETNAMESE;
}

// Helper function to check if the string is a ticket code (like BH-001, KH-0002, A-12, etc.)
// Helper function to check if the string is a ticket code (like BH-001, KH-0002, A-12, etc.)
inline bool isTicketCode(const char* text) {
  int len = strlen(text);
  if (len == 0 || len > 12) return false;
  
  bool hasDigits = false;
  for (int i = 0; text[i] != '\0'; i++) {
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
      colorName[colorNameLen] = '\0';
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
    if (strcmp(text, "---") != 0) {
        blink_counter = 3;
    } else {
        blink_counter = 0;
    }
    needRedraw = true;   
  }
}

// Display layout calculation
inline void updateDisplay() {
  if (!dma_display) return;
  dma_display->clearScreen();
  
  if (is_blank_state) return;

  if (show_startup_messages) {
      uint16_t wifi_color = g_wifi_connected ? 0x07E0 : 0xF800; // Green or Red
      dma_display->drawBitmap(12, 2, icon_wifi_16, 16, 16, wifi_color);
      
      uint16_t mqtt_color = g_mqtt_connected ? 0x07E0 : 0xF800;
      dma_display->drawBitmap(36, 2, icon_mqtt_16, 16, 16, mqtt_color);

      u8g2Fonts.setFont(u8g2_font_tom_thumb_4x6_tr);
      u8g2Fonts.setForegroundColor(0xFFFF);
      
      if (g_wifi_connected) {
          int w = u8g2Fonts.getUTF8Width(g_sta_ip);
          int xPos = (PANEL_RES_X - w) / 2;
          if (xPos < 0) xPos = 0;
          u8g2Fonts.setCursor(xPos, 28);
          u8g2Fonts.print(g_sta_ip);
      } else {
          u8g2Fonts.setCursor(12, 28);
          u8g2Fonts.print("No WiFi");
      }
      return;
  }
  
  if (strlen(textToDisplay) == 0) return;
  
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
    if (needRedraw || blink_counter > 0) {
      if (blink_counter > 0) {
          is_blank_state = false;
          updateDisplay();
          vTaskDelay(pdMS_TO_TICKS(400));
          
          is_blank_state = true;
          updateDisplay();
          vTaskDelay(pdMS_TO_TICKS(200));
          
          blink_counter--;
          if (blink_counter == 0) {
              is_blank_state = false;
              needRedraw = true;
          }
      } else {
          is_blank_state = false;
          updateDisplay();
          needRedraw = false;
          vTaskDelay(pdMS_TO_TICKS(50));
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}

// Task to read commands from Serial/UART
inline void serial_command_task(void *pvParameters) {
  char buffer[256];
  int idx = 0;
  while (1) {
    int c = getchar();
    if (c != EOF && c != 0xFF && c != 0) {
      if (c == '\n' || c == '\r') {
        if (idx > 0) {
          buffer[idx] = '\0';
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
