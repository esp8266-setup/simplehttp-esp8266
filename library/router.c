#include "simplehttp/http.h"

extern shttpConfig *shttpServerConfig;

static shttpRoute *shttp_find_route(char *path, shttpRequest *request) {

}

void shttp_exec_route(char *path, shttpRequest *request, int socket) {

//
// API
//

shttpRoute *shttp_route(shttpMethod method, char *path, shttpRouteCallback *callback) {
}