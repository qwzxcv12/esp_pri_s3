import re
with open('pri-ESP.cpp', 'r', encoding='utf-8') as f:
    c = f.read()

c = re.sub(r'add_device_log\(', 'ESP_LOGI(TAG, ', c)
c = re.sub(r'xTaskCreate\(startup_display_timeout_task, "display_timeout", 2048, NULL, 5, NULL\);\n', '', c)

with open('pri-ESP.cpp', 'w', encoding='utf-8') as f:
    f.write(c)
print('Fixed leftover errors in pri-ESP.cpp')
