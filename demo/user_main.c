#include <esp_common.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <simplehttp/http.h>
#define BUFF_SZ 200

void startup(void *userData);

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;
        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

void ICACHE_FLASH_ATTR wifi_event_handler_cb(System_Event_t *event) {
    static int running = 0;

    if (event->event_id == EVENT_STAMODE_GOT_IP) {
        if (running) {
            return;
        }
        running = 1;
        
        if (xTaskCreate(startup, "server", 250, NULL, 4, NULL) != pdPASS) {
            printf("HTTP Startup failed!");
        }
    }
}

// greet user by name, name is the last part of the request URL
static shttpResponse *helloName(shttpRequest *request) {
    // check if we have a parameter, this should be made sure by the lib,
    // but better safe than sorry
    if (request->numPathParameters == 1) {

        // create greeting message
        char responseBuffer[BUFF_SZ];
        int len = sprintf(
            responseBuffer,
            "Hello %s!",
            request->pathParameters[0]
        );

        // return plain text response
        return shttp_text_response(shttpStatusOK, responseBuffer);
    } else {
        // no parameter, bad request, no treats for you!
        return BAD_REQUEST;
    }
}

// return simple greeting without name
static shttpResponse *helloUnknown(shttpRequest *request) {
    return shttp_text_response(shttpStatusOK, "Hello you!");
}

// just a demo how to return a custom response for a wildcard
static shttpResponse *custom404(shttpRequest *request) {
    // just return an empty response with the code 404
    return NOT_FOUND;
}

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void) {
    printf("SDK version:%s\n", system_get_sdk_version());
    wifi_set_event_handler_cb(wifi_event_handler_cb);
}

void startup(void *userData) {
    shttpConfig config;

    // set a hostname, if a request with a different host header
    // arrives the server will automatically return 404
    config.hostName = "esp8266";

    // the port to use, default should be 80
    config.port = "80";

    // we don't care if the url ends with a slash
    config.appendSlashes = 1;

    // now define three routes
    config.routes = (shttpRoute *[]){
        // first route has a parameter
        GET("/hello/?", helloName),

        // second route is essentially the same as the first but will
        // only be called if there is no parameter
        GET("/hello", helloUnknown),

        // this route is a catchall and just returns 404
        GET("*", custom404),

        // we're finished, do not forget the sentinel
        NULL
    };

    // start the server, this never returns
    shttp_listen(&config);
}

