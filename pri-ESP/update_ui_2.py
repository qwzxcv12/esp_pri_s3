import re

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
    # 1. Update pri-ESP.cpp
    cd_esp_replacements = [
        # Add timeout task before event_handler
        (r'// ==================== WIFI EVENT HANDLER ====================',
         '''// ==================== WIFI EVENT HANDLER ====================
static void startup_display_timeout_task(void *pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(60000));
    if (show_startup_messages) {
        processMessage("---");
    }
    vTaskDelete(NULL);
}
'''),
        # Create task on GOT_IP
        (r'xEventGroupSetBits\(s_wifi_event_group, WIFI_CONNECTED_BIT\);\n        g_wifi_connected = true;\n        needRedraw = true;\n    \}',
         '''xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        g_wifi_connected = true;
        needRedraw = true;
        xTaskCreate(startup_display_timeout_task, "display_timeout", 2048, NULL, 5, NULL);
    }''')
    ]
    modify_file('pri-ESP.cpp', cd_esp_replacements)

    # 2. Update led_display.h
    led_display_replacements = [
        # Add globals
        (r'inline bool show_startup_messages = true;\ninline char g_sta_ip\[32\] = "";',
         'inline bool show_startup_messages = true;\ninline char g_sta_ip[32] = "";\ninline int blink_counter = 0;\ninline bool is_blank_state = false;'),
        
        # Update processMessage
        (r'if \(strlen\(text\) > 0\) \{\n    strcpy\(textToDisplay, text\);\n    needRedraw = true;   \n  \}',
         '''if (strlen(text) > 0) {
    strcpy(textToDisplay, text);
    if (strcmp(text, "---") != 0) {
        blink_counter = 3;
    } else {
        blink_counter = 0;
    }
    needRedraw = true;   
  }'''),
        
        # Update led_display_task
        (r'inline void led_display_task\(void \*pvParameters\) \{\n  while \(1\) \{\n    if \(needRedraw\) \{\n      updateDisplay\(\);\n      needRedraw = false;\n    \}\n    vTaskDelay\(pdMS_TO_TICKS\(50\)\); \n  \}\n\}',
         '''inline void led_display_task(void *pvParameters) {
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
}'''),
        
        # Update updateDisplay to handle is_blank_state
        (r'inline void updateDisplay\(\) \{\n  if \(!dma_display\) return;\n  dma_display->clearScreen\(\);\n  \n  if \(show_startup_messages\) \{',
         '''inline void updateDisplay() {
  if (!dma_display) return;
  dma_display->clearScreen();
  
  if (is_blank_state) return;

  if (show_startup_messages) {''')
    ]
    modify_file('led_display.h', led_display_replacements)
    
    print("UI Updates for timeout and blink applied successfully.")

if __name__ == '__main__':
    main()
