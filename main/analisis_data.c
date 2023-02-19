#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "esp_http_client.h"
#include "sdkconfig.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "board.h"
#include "esp_peripherals.h"
#include "periph_button.h"
#include "periph_wifi.h"
#include "periph_led.h"
#include "google_tts.h"
#include "google_sr.h"
#include "analisis_data.h"
#include "board.h"





#include "audio_idf_version.h"

#include <stdlib.h>

#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "esp_http_client.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

static const char *TAG = "ANALISIS_DATA";


char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
char *token = "";
//char *token = "eyJhbGciOiJIUzUxMiJ9.eyJzdWIiOiJhMzc2MTg1MzgyQGdtYWlsLmNvbSIsInVzZXJJZCI6ImViZWEzMWUwLTc5NzItMTFlZC1iNTgxLWNiMmRjOWJhOTg4ZCIsInNjb3BlcyI6WyJURU5BTlRfQURNSU4iXSwic2Vzc2lvbklkIjoiMTcxMDM0YzktYWQxNy00ZTdkLTljYmEtNGM2YWU1NzBiYmM3IiwiaXNzIjoidGhpbmdzYm9hcmQuaW8iLCJpYXQiOjE2NzUwMjY5NTUsImV4cCI6MTY3NjgyNjk1NSwiZmlyc3ROYW1lIjoi6bi_5b2mIiwibGFzdE5hbWUiOiLosKIiLCJlbmFibGVkIjp0cnVlLCJwcml2YWN5UG9saWN5QWNjZXB0ZWQiOnRydWUsImlzUHVibGljIjpmYWxzZSwidGVuYW50SWQiOiJlYTk0M2VkMC03OTcyLTExZWQtYjU4MS1jYjJkYzliYTk4OGQiLCJjdXN0b21lcklkIjoiMTM4MTQwMDAtMWRkMi0xMWIyLTgwODAtODA4MDgwODA4MDgwIn0.CeLScOr0LG6F09pirt-zFvivOdOb4WdqcqRZaBMH0tCOjFTzizBc8jVRbKUtWJBI50Stjy69Yuv9y8tRi4uZLw";
char revolve_api[5] = "";
long long time_ahora=1675355885672;
char *revolve_last = "";


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                if (evt->user_data) {
                    memcpy(evt->user_data + output_len, evt->data, evt->data_len);
                } else {
                    if (output_buffer == NULL) {
                        output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    memcpy(output_buffer + output_len, evt->data, evt->data_len);
                }
                output_len += evt->data_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
                // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
    }
    return ESP_OK;
}


void http_get_time(char *a){
    int content_length = 0;
    char str[800];
    memset(str, 0, sizeof(str));
    strcat(str, "Bearer ");
    strcat(str ,token);
    char url[150];
    memset(url, 0, sizeof(url));
    strcat(url, "https://thingsboard.cloud:443/api/plugins/telemetry/DEVICE/cf87adf0-dc76-11ec-b1ed-e5d3f0ce866e/values/timeseries?keys=");
    strcat(url, a);
    
    esp_http_client_config_t config = {
        .event_handler = _http_event_handler,
        .url = url,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_header(client, "Authorization", str);
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    } else {
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        } else {
            int data_read = esp_http_client_read_response(client, local_response_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
                cJSON* root = NULL;
                root = cJSON_Parse(local_response_buffer);

                cJSON* cjson_item =  cJSON_GetObjectItem(root,a);
                cJSON* cjson_result = cJSON_GetArrayItem(cjson_item,0);
                cJSON* cjson_return = cJSON_GetObjectItem(cjson_result,"ts");
                cJSON* cjson_return2 = cJSON_GetObjectItem(cjson_result,"value");
                time_ahora = cjson_return->valuedouble;
                revolve_last = cjson_return2->valuestring;
                ESP_LOGI(TAG, "[x] time ultima : %lld", time_ahora);
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }
    esp_http_client_cleanup(client);
    
}

static void http_get_data(char *a, long long time, bool isAyer, int range){
    int content_length = 0;
    char str[800];
    memset(str, 0, sizeof(str));
    strcat(str, "Bearer ");
    strcat(str ,token);
    char url[280];
    char time_start[50];
    char time_finish[50];
    sprintf(time_start, "%lld", time_ahora-time);

    if(isAyer){
        sprintf(time_finish, "%lld", time_ahora-86400000+1);
    }
    else{
        sprintf(time_finish, "%lld", time_ahora+1);
    }
    ESP_LOGI(TAG, "[x] time start... : %s", time_start);
    ESP_LOGI(TAG, "[x] time finish... : %s", time_finish);
    memset(url, 0, sizeof(url));
    strcat(url, "https://thingsboard.cloud:443/api/plugins/telemetry/DEVICE/cf87adf0-dc76-11ec-b1ed-e5d3f0ce866e/values/timeseries?keys=");
    strcat(url, a);
    strcat(url, "&startTs=");
    strcat(url, time_start);
    strcat(url, "&endTs=");
    strcat(url, time_finish);
    strcat(url, "&interval=");
    strcat(url, time_finish);
    strcat(url, "&agg=");
    if(range == 1){
        strcat(url, "MAX");
    }
    else if(range == 2){
        strcat(url, "MIN");
    }
    else if(range == 3 || range == 0){
        strcat(url, "AVG");
    }
    else{
        sprintf(revolve_api, "%s", revolve_last);
        return;
    }
    strcat(url, "&useStrictDataTypes=true");
    
    ESP_LOGI(TAG, "[x] url... : %s", url);

    esp_http_client_config_t config = {
        .event_handler = _http_event_handler,
        .url = url,
        .buffer_size_tx = 2048,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_header(client, "Authorization", str);
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    } else {
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        } else {
            int data_read = esp_http_client_read_response(client, local_response_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
                cJSON* root = NULL;
                root = cJSON_Parse(local_response_buffer);

                cJSON* cjson_item =  cJSON_GetObjectItem(root,a);
                
                ESP_LOGI(TAG, "[x] get datos : ..............");
                cJSON* subitem = cJSON_GetArrayItem(cjson_item, 0);
                cJSON* value = cJSON_GetObjectItem(subitem, "value");
                ESP_LOGI(TAG, "[x] get datos : %.1f", value->valuedouble);
                
                sprintf(revolve_api,  "%.1f", value->valuedouble);
                ESP_LOGI(TAG, "[x] get datos finish : %s", revolve_api);
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }
    esp_http_client_cleanup(client);
}

void http_get_token()
{
    ESP_LOGI(TAG, "[x] get token start");
    
    esp_http_client_config_t config = {
        .event_handler = _http_event_handler,
        .url = "https://thingsboard.cloud/api/auth/login",
        .user_data = local_response_buffer,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // POST
    const char *post_data = "{\"username\":\"studentupm2022@gmail.com\", \"password\":\"student\"}";
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    } else {
                ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
                cJSON* root = NULL;
                root = cJSON_Parse(local_response_buffer);
                cJSON* cjson_token = cJSON_GetObjectItem(root,"token");
                token = cjson_token->valuestring;
                ESP_LOGI(TAG, "[x] get token finish");
                ESP_LOGI(TAG, "[x] token: %s", token);
    }
    esp_http_client_cleanup(client);
}

char *send_text(int typeData, int time, int range){
    
    http_get_token();
    
    long long time_ms = 1;
    static char *text_return = "";
    static char test_text[128];
    ESP_LOGI(TAG, "send text...");
    if(typeData == 1){
        memset(test_text, 0, sizeof(test_text));
        strcat(test_text,"La temperatura ");
    }
    else if(typeData == 2){
        memset(test_text, 0, sizeof(test_text));
        strcat(test_text,"La humedad ");
    }
    else if(typeData == 3){
        memset(test_text, 0, sizeof(test_text));
        strcat(test_text,"El CO2 ");
    }
    else if(typeData == 4){
        memset(test_text, 0, sizeof(test_text));
        strcat(test_text,"La luminosidad ");
    }
    else{
        text_return = send_error(0);
        return text_return;
    }
    if(range == 4){
        //max
        strcat(test_text,"ahora");
    }
    else if(range == 1){
        //max
        strcat(test_text,"maxima");
    }
    else if(range == 2){
            //min
        strcat(test_text,"minima");
    }
    else if(range == 3 || range == 0){
            //medio
        strcat(test_text,"media");
        ESP_LOGI(TAG, "range: %d", range);
    }
    else{
        text_return = send_error(typeData);
        ESP_LOGI(TAG, "range: %d", range);
        return text_return;
    }

    if(range == 4){
        strcat(test_text," es ");
    }
    else if(time == 1){
        //hoy
        strcat(test_text," de hoy es ");
        time_ms = 86400001;
    }
    else if(time == 2){
        //ayer
        strcat(test_text," de ayer es ");
        time_ms = 86400000*2+1;
    }
    else if(time == 3){
        //semana
        strcat(test_text," de semana es ");
        time_ms = 86400000*7+1;
    }
    else if(time == 4){
        //mes
        strcat(test_text," de mes es ");
        time_ms = (long long)86400000*30+1;
    }
    else if(time == 5){
        //año
        strcat(test_text," de año es ");
        time_ms = (long long)86400000*365+1;
    }
    else{
        text_return = send_error(typeData);
        return text_return;
    }
    if(typeData == 1){
        http_get_time("temperature");
        http_get_data("temperature", time_ms,time == 2, range);
        ESP_LOGI(TAG, "temperatura: %s", revolve_api);
    }
    else if(typeData == 2){
        http_get_time("humidity");
        http_get_data("humidity", time_ms,time == 2, range);
        ESP_LOGI(TAG, "humedad: %s", revolve_api);
    }
    else if(typeData == 3){
        http_get_time("co2");
        http_get_data("co2", time_ms,time == 2, range);
        ESP_LOGI(TAG, "co2: %s", revolve_api);
    }
    else if(typeData == 4){
        http_get_time("light");
        http_get_data("light", time_ms,time == 2, range);
        ESP_LOGI(TAG, "luminosidad: %s", revolve_api);
    }
    else{
        text_return = send_error(0);
        return text_return;
    }

    strcat(test_text,revolve_api);
    ESP_LOGI(TAG, "text_return: %s ", test_text);
    text_return = test_text;
    
    return text_return;
}

char *send_error(int typeData){
    static char *text_error = "no entiendo, llamame otra vez";
    if(typeData == 1){
        text_error = "no se que temperatura quieres preguntar, preguntame otra vez";
    }
    else if(typeData == 2){
        text_error = "no se que humedad quieres preguntar, preguntame otra vez";
    }
    else if(typeData == 3){
        text_error = "no se que CO2 quieres preguntar, preguntame otra vez";
    }
    else if(typeData == 4){
        text_error = "no se que luminosidad quieres preguntar, preguntame otra vez";
    }
    else{
        text_error = "no entiendo, llamame otra vez";
    }
    return text_error;

}

