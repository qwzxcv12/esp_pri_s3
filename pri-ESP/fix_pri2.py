import re
with open('pri-ESP.cpp', 'r', encoding='utf-8') as f:
    c = f.read()

# Delete log_get_handler
c = re.sub(r'static esp_err_t log_get_handler.*?\}\n\n', '', c, flags=re.DOTALL)

# Delete log_data_get_handler
c = re.sub(r'static esp_err_t log_data_get_handler.*?\}\n\n', '', c, flags=re.DOTALL)

# Delete log_get registration
c = re.sub(r'        httpd_uri_t log_get = \{\n.*?\n        \};\n        httpd_register_uri_handler\(server, &log_get\);\n', '', c, flags=re.DOTALL)

# Delete log_data_get registration
c = re.sub(r'        httpd_uri_t log_data_get = \{\n.*?\n        \};\n        httpd_register_uri_handler\(server, &log_data_get\);\n', '', c, flags=re.DOTALL)

with open('pri-ESP.cpp', 'w', encoding='utf-8') as f:
    f.write(c)
print('Removed leftover log handlers')
