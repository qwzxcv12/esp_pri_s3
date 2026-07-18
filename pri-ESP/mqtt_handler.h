#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <mutex>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"

// Global variables (using inline to avoid multiple definition errors)
inline esp_mqtt_client_handle_t mqtt_client = NULL;
inline char g_dev_id[128] = {0};
inline char g_dev_key[128] = {0};
inline char g_mqtt_topic[256] = {0};

static const char *MQTT_TAG = "mqtt_qms";

inline void clean_broker_host(char* dst, const char* src, size_t dst_len) {
    const char* start = src;
    const char* proto_end = strstr(src, "://");
    if (proto_end) {
        start = proto_end + 3;
    }
    size_t i = 0;
    while (*start && *start != '/' && *start != ':' && i < dst_len - 1) {
        dst[i++] = *start++;
    }
    dst[i] = '\0';
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT CONNECTED to broker!");
        g_mqtt_connected = true;
        needRedraw = true;

        if (strlen(g_mqtt_topic) > 0) {
            char topic_buf[256];
            strncpy(topic_buf, g_mqtt_topic, sizeof(topic_buf) - 1);
            topic_buf[sizeof(topic_buf) - 1] = '\0';
            
            char *token = strtok(topic_buf, ",");
            while (token != NULL) {
                // Trim leading spaces
                while (*token == ' ') token++;
                // Trim trailing spaces
                size_t len = strlen(token);
                while (len > 0 && token[len - 1] == ' ') {
                    token[len - 1] = '\0';
                    len--;
                }

                if (strlen(token) > 0) {
                    // Subscribe to base topic
                    int msg_id1 = esp_mqtt_client_subscribe(client, token, 1);
                    ESP_LOGI(MQTT_TAG, "Subscribed to base: %s (msg_id=%d)", token, msg_id1);
                    
                    // Subscribe to device command topic
                    if (strlen(g_dev_id) > 0) {
                        char dev_topic[320];
                        snprintf(dev_topic, sizeof(dev_topic), "%s/%s/command", token, g_dev_id);
                        int msg_id2 = esp_mqtt_client_subscribe(client, dev_topic, 1);
                        ESP_LOGI(MQTT_TAG, "Subscribed to dev: %s (msg_id=%d)", dev_topic, msg_id2);
                    }
                }
                token = strtok(NULL, ",");
            }
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT DISCONNECTED. Reconnecting...");
        g_mqtt_connected = false;
        needRedraw = true;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT Subscribed successfully, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT Unsubscribed, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(MQTT_TAG, "MQTT Msg Rx on topic: %.*s", event->topic_len, event->topic);
        // Parse JSON
        {
            char *json_buf = (char*)malloc(event->data_len + 1);
            if (json_buf) {
                memcpy(json_buf, event->data, event->data_len);
                json_buf[event->data_len] = '\0';
                ESP_LOGI(MQTT_TAG, "Payload: %s", json_buf);

                cJSON *root = cJSON_Parse(json_buf);
                bool processed = false;
                if (root) {
                    cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
                    if (cmd && cJSON_IsString(cmd)) {
                        if (strcmp(cmd->valuestring, "display_ticket") == 0) {
                            cJSON *data = cJSON_GetObjectItem(root, "data");
                            if (data) {
                                const char *ticket = cJSON_GetStringValue(cJSON_GetObjectItem(data, "ticket"));
                                const char *counter = cJSON_GetStringValue(cJSON_GetObjectItem(data, "counter"));
                                const char *service = cJSON_GetStringValue(cJSON_GetObjectItem(data, "service"));
                                const char *color = cJSON_GetStringValue(cJSON_GetObjectItem(data, "color"));
                                const char *cust_name = cJSON_GetStringValue(cJSON_GetObjectItem(data, "cust_name"));

                                extern void processMessage(const char* command);
                                char disp_msg[128];
                                snprintf(disp_msg, sizeof(disp_msg), "%s %s", (color && strlen(color) > 0) ? color : "do", ticket ? ticket : "");
                                processMessage(disp_msg);
        show_startup_messages = false;
                                processed = true;

                                if (cust_name) {
                                    ESP_LOGI(MQTT_TAG, ">>> CALLING: Ticket=%s, Counter=%s, Service=%s, Customer=%s", 
                                        ticket ? ticket : "N/A", 
                                        counter ? counter : "N/A", 
                                        service ? service : "N/A",
                                        cust_name);
                                } else {
                                    ESP_LOGI(MQTT_TAG, ">>> CALLING: Ticket=%s, Counter=%s, Service=%s", 
                                        ticket ? ticket : "N/A", 
                                        counter ? counter : "N/A", 
                                        service ? service : "N/A");
                                }
                            }
                        } else if (strcmp(cmd->valuestring, "clear_display") == 0) {
                            extern void processMessage(const char* command);
                            processMessage("clear");
                            ESP_LOGI(MQTT_TAG, ">>> COMMAND: Clear screen");
                            processed = true;
                        }
                    }
                    cJSON_Delete(root);
                } else {
                    ESP_LOGI(MQTT_TAG, "Warning: Failed to parse JSON");
                }
                // Removed raw text fallback
                free(json_buf);
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTT_TAG, "MQTT Error occurred");
        break;

    default:
        break;
    }
}

static void mqtt_heartbeat_task(void *pvParameters)
{
    char hb_topic[256];
    snprintf(hb_topic, sizeof(hb_topic), "qms/heartbeat/%s", g_dev_id);

    char hb_payload[256];
    snprintf(hb_payload, sizeof(hb_payload), "{\"secret_key\":\"%s\"}", g_dev_key);

    ESP_LOGI(MQTT_TAG, "Heartbeat task active. Topic: %s", hb_topic);

    while (1) {
        if (mqtt_client) {
            int msg_id = esp_mqtt_client_publish(mqtt_client, hb_topic, hb_payload, 0, 0, 0);
            ESP_LOGI(MQTT_TAG, "Heartbeat sent (msg_id=%d)", msg_id);
        }
        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}

inline void mqtt_app_start(const char* broker, int port, const char* user, const char* pass, const char* topic)
{
    char clean_host[128] = {0};
    clean_broker_host(clean_host, broker, sizeof(clean_host));
    
    // Copy topic to global variable for subscribe when connected
    strncpy(g_mqtt_topic, topic, sizeof(g_mqtt_topic) - 1);
    g_mqtt_topic[sizeof(g_mqtt_topic) - 1] = '\0';

    ESP_LOGI(MQTT_TAG, "Starting MQTT Client to %s:%d (User: %s, Topics: %s)", clean_host, port, strlen(user) > 0 ? user : "None", g_mqtt_topic);

    char uri[256];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", clean_host, port);

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = uri;
    if (strlen(user) > 0) {
        mqtt_cfg.credentials.username = user;
    }
    if (strlen(pass) > 0) {
        mqtt_cfg.credentials.authentication.password = pass;
    }
    mqtt_cfg.network.reconnect_timeout_ms = 5000;

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);

    if (strlen(g_dev_id) > 0 && strlen(g_dev_key) > 0) {
        xTaskCreate(mqtt_heartbeat_task, "mqtt_hb", 4096, NULL, 5, NULL);
    } else {
        ESP_LOGI(MQTT_TAG, "Warning: Device credentials not set, Heartbeat disabled.");
    }
}

#endif // MQTT_HANDLER_H
