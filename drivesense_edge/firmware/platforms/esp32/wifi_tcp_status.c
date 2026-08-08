#include "status_transport.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "app_config.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "nvs_flash.h"

#define WIFI_CONNECTED_BIT BIT0

static TaskHandle_t s_task;
static EventGroupHandle_t s_wifi_events;
static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;
static char s_status[32] = "NORMAL";

static void status_transport_task(void *arg)
{
    (void)arg;
    int client = -1;
    char last_sent[sizeof(s_status)] = "";
    TickType_t last_heartbeat = 0;

    while (true) {
        xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE,
                            portMAX_DELAY);

        if (client < 0) {
            struct sockaddr_in address = {
                .sin_family = AF_INET,
                .sin_port = htons(APP_TCP_STATUS_PORT),
                .sin_addr.s_addr = htonl(INADDR_ANY),
            };
            int server = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
            if (server < 0) {
                vTaskDelay(pdMS_TO_TICKS(APP_WIFI_RETRY_DELAY_MS));
                continue;
            }
            int reuse = 1;
            setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
            if (bind(server, (struct sockaddr *)&address, sizeof(address)) != 0 ||
                listen(server, 1) != 0) {
                close(server);
                vTaskDelay(pdMS_TO_TICKS(APP_WIFI_RETRY_DELAY_MS));
                continue;
            }
            client = accept(server, NULL, NULL);
            close(server);
            if (client < 0) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            last_sent[0] = '\0';
            last_heartbeat = 0;
        }

        char status[sizeof(s_status)];
        portENTER_CRITICAL(&s_status_lock);
        memcpy(status, s_status, sizeof(status));
        portEXIT_CRITICAL(&s_status_lock);
        status[sizeof(status) - 1] = '\0';
        TickType_t now = xTaskGetTickCount();
        bool changed = strcmp(status, last_sent) != 0;
        bool heartbeat = (now - last_heartbeat) >= pdMS_TO_TICKS(1000);
        if (changed || heartbeat) {
            char line[sizeof(status) + 2];
            int length = snprintf(line, sizeof(line), "%s\n", status);
            if (send(client, line, length, 0) < 0) {
                close(client);
                client = -1;
                continue;
            }
            memcpy(last_sent, status, sizeof(last_sent));
            last_heartbeat = now;
        }

        char probe;
        int received = recv(client, &probe, sizeof(probe), MSG_DONTWAIT);
        if (received == 0 || (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            close(client);
            client = -1;
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

void status_transport_start(void)
{
    if (s_task != NULL) {
        return;
    }

    s_wifi_events = xEventGroupCreate();
    if (s_wifi_events == NULL) {
        return;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        if (nvs_flash_erase() == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK) {
        return;
    }

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return;
    }
    if (esp_netif_create_default_wifi_sta() == NULL) {
        return;
    }

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&wifi_config) != ESP_OK) {
        return;
    }
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    wifi_config_t station_config = {0};
    strncpy((char *)station_config.sta.ssid, APP_WIFI_SSID,
            sizeof(station_config.sta.ssid) - 1);
    strncpy((char *)station_config.sta.password, APP_WIFI_PASSWORD,
            sizeof(station_config.sta.password) - 1);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &station_config);
    esp_wifi_start();
    xTaskCreate(status_transport_task, "wifi_tcp_status", 4096, NULL, 4, &s_task);
}

void status_transport_publish(const char *status)
{
    if (status == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_status_lock);
    strncpy(s_status, status, sizeof(s_status) - 1);
    s_status[sizeof(s_status) - 1] = '\0';
    portEXIT_CRITICAL(&s_status_lock);
}
