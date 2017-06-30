#include "simplehttp/http.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

void shttp_write_response(shttpResponse *response, int socket) {
    char *responseIntro = NULL;
    switch(response->responseCode) {
        case shttpStatusOK:
            responseIntro = "200 Ok";
            break;
        case shttpStatusCreated:
            responseIntro = "201 Created";
            break;
        case shttpStatusAccepted:
            responseIntro = "202 Accepted";
            break;
        case shttpStatusNoContent:
            responseIntro = "204 No content";
            break;
    
        case shttpStatusMovedPermanently:
            responseIntro = "301 Redirect";
            break;
        case shttpStatusFound:
            responseIntro = "302 Found";
            break;
        case shttpStatusNotModified:
            responseIntro = "304 Not modified";
            break;

        case shttpStatusBadRequest:
            responseIntro = "400 Bad request";
            break;
        case shttpStatusUnauthorized:
            responseIntro = "401 Unauthorized";
            break;
        case shttpStatusForbidden:
            responseIntro = "403 Forbidden Ok";
            break;
        case shttpStatusNotFound:
            responseIntro = "404 Not found";
            break;
        case shttpStatusNotAcceptable:
            responseIntro = "406 Not acceptable";
            break;
        case shttpStatusConflict:
            responseIntro = "409 Conflict";
            break;
        case shttpStatusRequestURITooLong:
            responseIntro = "414 Request URI too long";
            break;

        case shttpStatusInternalError:
            responseIntro = "500 Internal server error";
            break;
        case shttpStatusNotImplemented:
            responseIntro = "501 Not implemented";
            break;
        case shttpStatusBadGateway:
            responseIntro = "502 Bad gateway";
            break;
        case shttpStatusServiceUnavailable:
            responseIntro = "503 Service unavailable";
            break;
    }

    // send status line
    send(socket, "HTTP/1.1 ", 9, 0);
    send(socket, responseIntro, strlen(responseIntro), 0);
    send(socket, "\r\n", 2, 0);

    // if there are any headers send them first
    if (response->headers) {
        shttpHeader *header = *response->headers;
        while(header != NULL) {
            send(socket, header->name, strlen(header->name), 0);
            send(socket, ": ", 2, 0);
            send(socket, header->value, strlen(header->value), 0);
            send(socket, "\r\n", 2, 0);
            header++;
        }
    }
    // Send a connection close header as we close the connection anyway
    send(socket, "Connection: close\r\n", 19, 0);

    // if we know the body length add a content-length header
    uint32_t contentLength = 0;
    if (response->body) {
        contentLength = (response->bodyLen > 0) ? response->bodyLen : strlen(response->body);
    }
    if (response->bodyCallback) {
        contentLength = (response->bodyLen > 0) ? response->bodyLen : 0;
    }
    if (contentLength > 0) {
        char *tmp = malloc(18 + 8); // max 99 MB
        sprintf(tmp, "Content-Length: %d\r\n", contentLength);
        send(socket, tmp, strlen(tmp), 0);
        free(tmp);
    }

    // finish header block
    send(socket, "\r\n", 2, 0);

    // send body
    if (response->body) {
        // body data available, direct send
        send(socket, response->body, contentLength, 0);
    }
    if (response->bodyCallback) {
        // callback option, repeatedly call callback and stream out data
        uint32_t position = 0;
        uint32_t chunkLen = 0;
        while (1) {
            char *chunk = response->bodyCallback(position, &chunkLen, response->callbackUserData);
            if (!chunk) {
                // if chunk is NULL, callback is finished, clean up
                response->cleanupCallback(response->callbackUserData);
                break;
            }

            // send the chunk and free the memory
            bool fault = false;
            while (1) {
                int bytes = send(socket, chunk, chunkLen, 0);
                if (bytes == 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    fault = true;
                    break;
                }
                break;
            }
            free(chunk);

            // increment position
            position += chunkLen;

            if (fault) {
                return; // Network disconnected, cancel sending data
            }
        }
    }
}

//
// API
//

void shttp_response_add_headers(shttpResponse *response, ...) {
    char *name, *value;
    uint8_t headerCount;
    va_list ap;

    // count number of headers
    va_start(ap, response);
    name = va_arg(ap, char *);
    while(*name) {
        value = va_arg(ap, char *);
        if (value == NULL) {
            va_end(ap);
            return; // count not divisible by 2? bail out!
        }

        // next
        headerCount++;
        name = va_arg(ap, char *);
    }
    va_end(ap);

    // no headers, nothing to do
    if (headerCount == 0) {
        return;
    }

    // allocate memory
    response->headers = malloc((headerCount + 1) * sizeof(shttpHeader *));
    if (!response->headers) {
        return; // Out of memory
    }
    response->headers[headerCount] = NULL; // sentinel

    va_start(ap, response);
    uint8_t i = 0;
    name = va_arg(ap, char *);
    while(*name) {
        value = va_arg(ap, char *);
        if (value == NULL) {
            va_end(ap);
            return;
        }

        // copy header name and value into struct
        uint16_t len = strlen(name);
        response->headers[i]->name = malloc(len);
        memcpy(response->headers[i]->name, name, len);

        len = strlen(value);
        response->headers[i]->value = malloc(len);
        memcpy(response->headers[i]->value, value, len);

        // next
        i++;
        name = va_arg(ap, char *);
    }
    va_end(ap);
}

shttpResponse *shttp_empty_response(shttpStatusCode status) {
    shttpResponse *response = malloc(sizeof(shttpResponse));
    memset(response, 0, sizeof(shttpResponse));

    response->responseCode = status;
    return response;
}

#if SHTTP_CJSON
shttpResponse *shttp_json_response(shttpStatusCode status, cJSON *json) {
    shttpResponse *response = shttp_empty_response(status);
    shttp_response_add_headers(response, "Content-Type", "application/json", NULL);
    response->body = cJSON_Print(json);

    return response;
}
#endif /* SHTTP_CJSON */

shttpResponse *shttp_html_response(shttpStatusCode status, char *html) {
    shttpResponse *response = shttp_empty_response(status);
    shttp_response_add_headers(response, "Content-Type", "text/html", NULL);
    response->body = html;

    return response;
}

shttpResponse *shttp_text_response(shttpStatusCode status, char *text) {
    shttpResponse *response = shttp_empty_response(status);
    shttp_response_add_headers(response, "Content-Type", "text/plain", NULL);
    response->body = text;

    return response;
}

shttpResponse *shttp_download_response(shttpStatusCode status, char *buffer, uint32_t len, char *filename) {
    shttpResponse *response = shttp_empty_response(status);

    // download means an attachment header
    char *disposition = "attachment";
    
    if (filename) {
        // if we have a filename add it to the header
        uint8_t len = 23 + strlen(filename) + 1;
        char *tmp = malloc(len);
        sprintf(tmp, "attachment; filename=\"%s\"", filename);
        disposition = tmp;
    }

    shttp_response_add_headers(response,
        "Content-Type", "application/octet-stream",
        "Content-Disposition", disposition,
        NULL);

    // response body needs length as it is probably binary
    response->body = buffer;
    response->bodyLen = len;

    // free disposition if constructed
    if (filename) {
        free(disposition);
    }

    return response;

}

shttpResponse *shttp_download_callback_response(shttpStatusCode status, uint32_t len, char *filename, shttpBodyCallback *callback, void *userData, shttpCleanupCallback *cleanup) {
    shttpResponse *response = shttp_download_response(status, NULL, 0, filename);
    response->bodyCallback = callback;
    response->callbackUserData = userData;
    response->cleanupCallback = cleanup;

    return response;
}
