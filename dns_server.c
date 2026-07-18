#include <string.h>
#include "dns_server.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "dns_server";
static TaskHandle_t s_dns_task = NULL;
static int s_sock = -1;

#define DNS_PORT        53
#define DNS_MAX_LEN     512

// Cấu trúc header DNS tối giản
typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} dns_header_t;

static void dns_server_task(void *pvParameters)
{
    uint32_t resolved_ip = (uint32_t)(uintptr_t)pvParameters;

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DNS_PORT);

    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket");
        vTaskDelete(NULL);
        return;
    }
    if (bind(s_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Socket unable to bind on port %d", DNS_PORT);
        close(s_sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "DNS captive portal server started on port %d", DNS_PORT);

    uint8_t rx_buf[DNS_MAX_LEN];
    uint8_t tx_buf[DNS_MAX_LEN];

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(s_sock, rx_buf, sizeof(rx_buf), 0,
                            (struct sockaddr *)&source_addr, &socklen);
        if (len < (int)sizeof(dns_header_t)) {
            continue;
        }

        dns_header_t *header = (dns_header_t *)rx_buf;

        // Chỉ trả lời nếu là query (QR bit = 0)
        if ((ntohs(header->flags) & 0x8000) != 0) {
            continue;
        }

        // Copy request sang buffer trả lời
        int reply_len = len;
        memcpy(tx_buf, rx_buf, len);
        dns_header_t *reply_header = (dns_header_t *)tx_buf;

        // flags: response, no error, recursion available
        reply_header->flags = htons(0x8180);
        reply_header->an_count = header->qd_count; // trả lời cho mỗi câu hỏi

        // Câu hỏi bắt đầu ngay sau header, giữ nguyên
        uint8_t *ptr = tx_buf + sizeof(dns_header_t);
        // Nhảy qua phần QNAME + QTYPE(2) + QCLASS(2) của câu hỏi đầu tiên
        while (*ptr != 0 && (ptr - tx_buf) < len) {
            ptr += (*ptr) + 1;
        }
        ptr += 1 + 4; // qua byte 0 kết thúc tên + QTYPE + QCLASS

        // Ghi answer record cho từng câu hỏi (thường chỉ có 1)
        uint16_t qd_count = ntohs(header->qd_count);
        for (int i = 0; i < qd_count && (ptr - tx_buf) + 16 <= DNS_MAX_LEN; i++) {
            *ptr++ = 0xC0; *ptr++ = 0x0C;           // Pointer trỏ về tên trong câu hỏi
            *ptr++ = 0x00; *ptr++ = 0x01;           // TYPE = A
            *ptr++ = 0x00; *ptr++ = 0x01;           // CLASS = IN
            *ptr++ = 0x00; *ptr++ = 0x00; *ptr++ = 0x00; *ptr++ = 0x3C; // TTL = 60s
            *ptr++ = 0x00; *ptr++ = 0x04;           // RDLENGTH = 4
            memcpy(ptr, &resolved_ip, 4);           // RDATA = resolved_ip
            ptr += 4;
            reply_len = ptr - tx_buf;
        }

        sendto(s_sock, tx_buf, reply_len, 0,
               (struct sockaddr *)&source_addr, socklen);
    }
}

esp_err_t dns_server_start(uint32_t resolved_ip)
{
    if (s_dns_task != NULL) {
        return ESP_OK; // đã chạy
    }
    BaseType_t ok = xTaskCreate(dns_server_task, "dns_server", 4096,
                                 (void *)(uintptr_t)resolved_ip, 5, &s_dns_task);
    return ok == pdPASS ? ESP_OK : ESP_FAIL;
}

void dns_server_stop(void)
{
    if (s_sock >= 0) {
        close(s_sock);
        s_sock = -1;
    }
    if (s_dns_task != NULL) {
        vTaskDelete(s_dns_task);
        s_dns_task = NULL;
    }
}
