#include "simplehttp/http.h"

shttpResponse *shttp_empty_response(shttpStatusCode status) {
    
}

#if SHTTP_CJSON
shttpResponse *shttp_json_response(shttpStatusCode status, cJSON *json) {

}
#endif /* SHTTP_CJSON */

shttpResponse *shttp_html_response(shttpStatusCode status, char *html) {

}

shttpResponse *shttp_text_response(shttpStatusCode status, char *text) {

}

shttpResponse *shttp_download_response(shttpStatusCode status, uint32_t len, char *buffer) {

}

shttpResponse *shttp_download_callback_response(shttpStatusCode status, uint32_t len, shttpBodyCallback callback, void *userData, shttpCleanupCallback cleanup) {
    
}
