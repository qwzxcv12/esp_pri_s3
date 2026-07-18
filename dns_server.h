#pragma once
#include "esp_err.h"

// Khởi động DNS server captive portal (chạy trong 1 task riêng)
// Mọi query DNS sẽ được trả lời bằng resolved_ip
esp_err_t dns_server_start(uint32_t resolved_ip);

// Dừng DNS server
void dns_server_stop(void);
