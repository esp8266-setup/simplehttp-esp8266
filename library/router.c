#include "simplehttp/http.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "response.h"

extern shttpConfig *shttpServerConfig;

static shttpRoute *shttp_find_route(char *path, shttpRequest *request) {
    uint8_t pathLen = strlen(path);

    if (shttpServerConfig->appendSlashes) {
        if (path[pathLen - 1] == '/') {
            path[pathLen - 1] = '\0';
            pathLen--;
        }
    }

    shttpRoute *route = shttpServerConfig->routes[0];
    while(route != NULL) {
        uint8_t routeLen = strlen(route->path);

        bool found = true;
        for (uint8_t pathIndex = 0, routeIndex = 0; pathIndex < pathLen; pathIndex++) {
            if (routeIndex > routeLen) {
                found = false;
                break;
            }

            if (route->path[routeIndex] == '?') {
                // found parameter, skip path to next slash or end
                for(uint8_t i = 0; i < pathLen - pathIndex; i++) {
                    if (path[pathIndex + i] == '/') {
                        pathIndex += i - 1;
                        break;
                    }
                }
                routeIndex++;
                continue;
            } else if (route->path[routeIndex] == '*') {
                found = true;
                break;
            } else if (route->path[routeIndex] != path[pathIndex]) {
                found = false;
                break;
            }
            routeIndex++;
        }

        if (found) {
            break;
        }

        route++;
    }

    return route;
}

static void shttp_parse_url_parameters(char *path, shttpRoute *route, shttpRequest *request) {    
    uint8_t pathLen = strlen(path);
    uint8_t routeLen = strlen(route->path);
    for (uint8_t pathIndex = 0, routeIndex = 0; pathIndex < pathLen; pathIndex++) {
        if (routeIndex > routeLen) {
            break;
        }

        if (route->path[routeIndex] == '?') {
            // found parameter, find next slash
            for(uint8_t i = 0; i < pathLen - pathIndex; i++) {
                if (path[pathIndex + i] == '/') {
                    // copy parameter value
                    char *param = malloc(i);
                    memcpy(param, path + pathIndex, i - 1);
                    param[i - 1] = '\0';

                    // realloc parameter array and append param
                    request->pathParameters = realloc(request->pathParameters, (request->numPathParameters + 1) * sizeof(char *));
                    request->pathParameters[request->numPathParameters] = param;
                    request->numPathParameters++;

                    // skip over the parameter and continue parsing
                    pathIndex += i - 1;
                    break;
                }
            }
        } else if (route->path[routeIndex] == '*') {
            break;
        }

        routeIndex++;
    }

}

void shttp_exec_route(char *path, shttpRequest *request, int socket) {
    // find a route
    shttpRoute *route = shttp_find_route(path, request);

    // no route found return 404
    if (!route) {
        shttp_write_response(shttp_empty_response(shttpStatusNotFound), socket);
    }

    // parse url parameters
    shttp_parse_url_parameters(path, route, request);

    // call callback and return response
    shttp_write_response(route->callback(request), socket);
}

//
// API
//

shttpRoute *shttp_route(shttpMethod method, char *path, shttpRouteCallback *callback) {
    shttpRoute *route = malloc(sizeof(shttpRoute));
    route->allowedMethods = method;
    route->path = path;
    route->callback = callback;

    return route;
}