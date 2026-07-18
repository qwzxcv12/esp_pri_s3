import re
import sys

def modify_file(filepath, replacements):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    for pattern, repl in replacements:
        new_content = re.sub(pattern, repl, content, flags=re.DOTALL)
        if new_content == content:
            print(f"Warning: Regex pattern not found in {filepath}: {pattern[:50]}...")
        content = new_content
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

def main():
    # 1. Update led_display.h
    led_display_replacements = [
        # Add globals and icons
        (r'inline char g_sta_ip\[32\] = "";\n', 
         'inline char g_sta_ip[32] = "";\nextern bool g_wifi_connected;\nextern bool g_mqtt_connected;\n\n'
         'inline const uint8_t icon_wifi[8] = {0x00, 0x3C, 0x42, 0x81, 0x3C, 0x42, 0x00, 0x18};\n'
         'inline const uint8_t icon_mqtt[8] = {0x3C, 0x42, 0x99, 0xA5, 0x81, 0x99, 0x42, 0x3C};\n'),
        
        # Replace updateDisplay
        (r'inline void updateDisplay\(\) \{.*?return;\n  \}\n  \n  int16_t yPos',
         '''inline void updateDisplay() {
  if (!dma_display) return;
  dma_display->clearScreen();
  
  if (show_startup_messages) {
      uint16_t wifi_color = g_wifi_connected ? 0x07E0 : 0xF800; // Green or Red
      dma_display->drawBitmap(16, 2, icon_wifi, 8, 8, wifi_color);
      
      uint16_t mqtt_color = g_mqtt_connected ? 0x07E0 : 0xF800;
      dma_display->drawBitmap(40, 2, icon_mqtt, 8, 8, mqtt_color);

      u8g2Fonts.setFont(u8g2_font_tom_thumb_4x6_tr);
      u8g2Fonts.setForegroundColor(0xFFFF);
      
      if (g_wifi_connected) {
          int w = u8g2Fonts.getUTF8Width(g_sta_ip);
          int xPos = (PANEL_RES_X - w) / 2;
          if (xPos < 0) xPos = 0;
          u8g2Fonts.setCursor(xPos, 22);
          u8g2Fonts.print(g_sta_ip);
      } else {
          u8g2Fonts.setCursor(12, 22);
          u8g2Fonts.print("No WiFi");
      }
      return;
  }
  
  if (strlen(textToDisplay) == 0) return;
  
  int16_t yPos''')
    ]
    modify_file('led_display.h', led_display_replacements)

    # 2. Update pri-ESP.cpp
    cd_esp_replacements = [
        # Add globals
        (r'#include "mqtt_handler.h"', '#include "mqtt_handler.h"\n\nbool g_wifi_connected = false;\nbool g_mqtt_connected = false;\n'),
        
        # Update wifi disconnect
        (r'\} else if \(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED\) \{',
         '} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {\n        g_wifi_connected = false;\n        needRedraw = true;\n'),
        
        # Update wifi connect
        (r'ESP_LOGI\(TAG, "Successfully got IP: %s", g_sta_ip\);\n        s_retry_num = 0;\n        xEventGroupSetBits\(s_wifi_event_group, WIFI_CONNECTED_BIT\);\n        if \(show_startup_messages\) \{.*?\}',
         'ESP_LOGI(TAG, "Successfully got IP: %s", g_sta_ip);\n        s_retry_num = 0;\n        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);\n        g_wifi_connected = true;\n        needRedraw = true;'),
        
        # Set show_startup_messages = false when receiving messages
        (r'processMessage\(disp_msg\);', 'processMessage(disp_msg);\n    show_startup_messages = false;'),
        
        # Delete control handler
        (r'static esp_err_t control_get_handler.*?\}\n\n', ''),
        
        # Delete control URI registration
        (r'        httpd_uri_t control_get = \{\n.*?\n        \};\n        httpd_register_uri_handler\(server, &control_get\);\n', '')
    ]
    modify_file('pri-ESP.cpp', cd_esp_replacements)

    # 3. Update mqtt_handler.h
    mqtt_replacements = [
        (r'case MQTT_EVENT_CONNECTED:\n        ESP_LOGI\(MQTT_TAG, "MQTT CONNECTED to broker!"\);\n',
         'case MQTT_EVENT_CONNECTED:\n        ESP_LOGI(MQTT_TAG, "MQTT CONNECTED to broker!");\n        g_mqtt_connected = true;\n        needRedraw = true;\n'),
        (r'case MQTT_EVENT_DISCONNECTED:\n        ESP_LOGI\(MQTT_TAG, "MQTT DISCONNECTED. Reconnecting\.\.\."\);\n        break;',
         'case MQTT_EVENT_DISCONNECTED:\n        ESP_LOGI(MQTT_TAG, "MQTT DISCONNECTED. Reconnecting...");\n        g_mqtt_connected = false;\n        needRedraw = true;\n        break;'),
        (r'processMessage\(disp_msg\);', 'processMessage(disp_msg);\n        show_startup_messages = false;')
    ]
    modify_file('mqtt_handler.h', mqtt_replacements)

    # 4. Update wifi_config_html.h
    html_replacements = [
        (r'<a href="/control".*?</a>\n', ''),
        (r'<a href="/log".*?</a>\n', '')
    ]
    modify_file('wifi_config_html.h', html_replacements)
    
    print("UI Updates applied successfully.")

if __name__ == '__main__':
    main()
