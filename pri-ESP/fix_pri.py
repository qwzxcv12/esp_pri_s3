import re
with open('pri-ESP.cpp', 'r', encoding='utf-8') as f:
    c = f.read()

# 1. Remove log_page and send_log_template_chunked
c = re.sub(r'const char\* log_page =.*?R"====\((.*?)\)====";\n', '', c, flags=re.DOTALL)
c = re.sub(r'static esp_err_t send_log_template_chunked.*?\}\n\n', '', c, flags=re.DOTALL)

# 2. Remove log_get_handler and log_data_get_handler
c = re.sub(r'static esp_err_t log_get_handler.*?return ESP_OK;\n\}\n\n', '', c, flags=re.DOTALL)
c = re.sub(r'static esp_err_t log_data_get_handler.*?return ESP_OK;\n\}\n\n', '', c, flags=re.DOTALL)

# 3. Remove registration of log handlers in start_webserver
c = re.sub(r'        httpd_uri_t log_get.*?httpd_register_uri_handler\(server, &log_get\);\n', '', c, flags=re.DOTALL)
c = re.sub(r'        httpd_uri_t log_data_get.*?httpd_register_uri_handler\(server, &log_data_get\);\n', '', c, flags=re.DOTALL)

# 4. Remove startup_display_timeout_task
c = re.sub(r'static void startup_display_timeout_task\(void \*pvParameters\);\n', '', c)
c = re.sub(r'static void startup_display_timeout_task\(void \*pvParameters\) \{.*?\}\n\n', '', c, flags=re.DOTALL)
c = re.sub(r'            static bool timer_started = false;\n            if \(!timer_started\) \{\n                xTaskCreate\(startup_display_timeout_task, "display_timeout", 2048, NULL, 5, NULL\);\n                timer_started = true;\n            \}\n', '', c, flags=re.DOTALL)

with open('pri-ESP.cpp', 'w', encoding='utf-8') as f:
    f.write(c)
print('Removed log endpoints from pri-ESP.cpp')
