/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "mqtt_client.h"


#include "lwip/err.h"
#include "lwip/sys.h"



/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_DISCONNECTED_BIT      BIT2

#define CHANNEL1 GPIO_NUM_4
#define CHANNEL2 GPIO_NUM_5
//#define CHANNEL1 GPIO_NUM_2
//#define CHANNEL2 GPIO_NUM_16


static const char *TAG = "wifi station";
static char strTmp[128];
static int msg_id;
static TaskHandle_t xHandleReconnect = NULL;



static uint8_t base_mac_addr[6]={0};
static char string_mac_addr[18];
static char toppic_channel_1[30],toppic_channel_2[30];



static void mqtt_app_start(void);

//BaseType_t xReturned;
TaskHandle_t xHandle = NULL;
static EventBits_t returnBit;
static esp_mqtt_client_handle_t client;


void wifi_reconnect(void * pvParameters)
{
	while(1)
	 {
		ESP_LOGE(TAG,"TRY CONNECT");
		esp_wifi_connect();
		vTaskDelay(30000/portTICK_RATE_MS);
	 }
}






static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
	returnBit=xEventGroupGetBits(s_wifi_event_group);
	ESP_LOGE(TAG,"EVENT HANDLET!!");
	ESP_LOGI(TAG,event_base);
	ESP_LOGI(TAG," %i",event_id);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    	esp_wifi_connect();
    	//xTaskCreate(&wifi_reconnect,"reconnect",2048,NULL,5,&xHandleReconnect);
    }


    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    	esp_wifi_connect();

    }


    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    	 //xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    	 //vTaskDelete( xHandleReconnect );
    }







    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    	esp_mqtt_client_start(client);

    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
        	esp_mqtt_client_stop(client);

        }







}






void wifi_init_sta(void)
{


    tcpip_adapter_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,ESP_EVENT_ANY_ID, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "own-88-IOT",
            .password = "z2BEZbBr7M7qfuv8",
			.threshold.authmode = WIFI_AUTH_WPA2_PSK

        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );


    ESP_ERROR_CHECK(esp_wifi_start() );


}





static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    //esp_mqtt_client_handle_t client = event->client;

    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client,toppic_channel_1, 0);
            msg_id = esp_mqtt_client_subscribe(client,toppic_channel_2, 0);
            msg_id = esp_mqtt_client_subscribe(client,"/system", 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
        	esp_wifi_disconnect();
        	esp_wifi_connect();
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");

            break;
        case MQTT_EVENT_SUBSCRIBED:

            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            memcpy(strTmp,event->topic,event->topic_len);
            strTmp[event->topic_len]='\0';
            int tmpLevel;

            if(strcmp(strTmp,toppic_channel_1)==0){
            	ESP_LOGI(TAG, "CHANNEL1");
            	memcpy(strTmp,event->data,event->data_len);
            	strTmp[event->data_len]='\0';
            	tmpLevel=atol(strTmp);
            	gpio_set_level(CHANNEL1,tmpLevel);
            }
            if(strcmp(strTmp,toppic_channel_2)==0){
            	ESP_LOGI(TAG, "CHANNEL2");
               	memcpy(strTmp,event->data,event->data_len);
              	strTmp[event->data_len]='\0';
               	tmpLevel=atol(strTmp);
               	gpio_set_level(CHANNEL2,tmpLevel);
            }




            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
        	ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}






static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}



static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .host = "192.168.21.1",
		.port=1883,
		.username="client",
		.password="owX5Q58y"
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
}





void app_main()
{
	s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    esp_efuse_mac_get_default(base_mac_addr);
    snprintf(string_mac_addr, sizeof(string_mac_addr), "%02x:%02x:%02x:%02x:%02x:%02x",
    		base_mac_addr[0], base_mac_addr[1], base_mac_addr[2], base_mac_addr[3],base_mac_addr[4],base_mac_addr[5]);

    strcpy(toppic_channel_1,"/");
    strcat(toppic_channel_1,string_mac_addr);
    strcat(toppic_channel_1,"/channel1");

    strcpy(toppic_channel_2,"/");
    strcat(toppic_channel_2,string_mac_addr);
    strcat(toppic_channel_2,"/channel2");


    ESP_LOGE(TAG,toppic_channel_1);
    gpio_set_direction(CHANNEL1,GPIO_MODE_DEF_OUTPUT);
    gpio_set_direction(CHANNEL2,GPIO_MODE_DEF_OUTPUT);
    wifi_init_sta();
    mqtt_app_start();
}
