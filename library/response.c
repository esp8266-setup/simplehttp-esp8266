#include "simplehttp/http.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <unistd.h>

#include "debug.h"

ICACHE_FLASH_ATTR void shttp_write_response(shttpResponse *response, struct netconn *conn) {
    err_t err;

    const char *responseIntro = NULL;
    switch(response->responseCode) {
        case shttpStatusOK:
            responseIntro = FSTR("200 Ok");
            break;
        case shttpStatusCreated:
            responseIntro = FSTR("201 Created");
            break;
        case shttpStatusAccepted:
            responseIntro = FSTR("202 Accepted");
            break;
        case shttpStatusNoContent:
            responseIntro = FSTR("204 No content");
            break;

        case shttpStatusMovedPermanently:
            responseIntro = FSTR("301 Redirect");
            break;
        case shttpStatusFound:
            responseIntro = FSTR("302 Found");
            break;
        case shttpStatusNotModified:
            responseIntro = FSTR("304 Not modified");
            break;

        case shttpStatusBadRequest:
            responseIntro = FSTR("400 Bad request");
            break;
        case shttpStatusUnauthorized:
            responseIntro = FSTR("401 Unauthorized");
            break;
        case shttpStatusForbidden:
            responseIntro = FSTR("403 Forbidden");
            break;
        case shttpStatusNotFound:
            responseIntro = FSTR("404 Not found");
            break;
        case shttpStatusNotAcceptable:
            responseIntro = FSTR("406 Not acceptable");
            break;
        case shttpStatusConflict:
            responseIntro = FSTR("409 Conflict");
            break;
        case shttpStatusRequestURITooLong:
            responseIntro = FSTR("414 Request URI too long");
            break;

        case shttpStatusInternalError:
            responseIntro = FSTR("500 Internal server error");
            break;
        case shttpStatusNotImplemented:
            responseIntro = FSTR("501 Not implemented");
            break;
        case shttpStatusBadGateway:
            responseIntro = FSTR("502 Bad gateway");
            break;
        case shttpStatusServiceUnavailable:
            responseIntro = FSTR("503 Service unavailable");
            break;
    }

    LOG(TRACE, "shttp: sending response '%s'", responseIntro);

    // send status line
    netconn_write(conn, FSTR("HTTP/1.1 "), 9, NETCONN_NOCOPY);
    netconn_write(conn, responseIntro, strlen(responseIntro), NETCONN_NOCOPY);
    netconn_write(conn, FSTR("\r\n"), 2, NETCONN_NOCOPY);

    // if there are any headers send them first
    if (response->headerCount > 0) {
        for(uint8_t i = 0; i < response->headerCount; i++) {
            shttpHeader *header = &(response->headers[i]);
            LOG(TRACE, "shttp: sending header '%s: %s' (%d and %d bytes)", header->name, header->value, strlen(header->name), strlen(header->value));

            netconn_write(conn, header->name, strlen(header->name), NETCONN_COPY);
            netconn_write(conn, FSTR(": "), 2, NETCONN_NOCOPY);
            netconn_write(conn, header->value, strlen(header->value), NETCONN_COPY);
            netconn_write(conn, FSTR("\r\n"), 2, NETCONN_NOCOPY);
        
            free(header->name);
            free(header->value);
        }
        free(response->headers);
    }
    // Send a connection close header as we close the connection anyway

    netconn_write(conn, FSTR("Connection: close\r\n"), 19, NETCONN_NOCOPY);

    // if we know the body length add a content-length header
    uint32_t contentLength = 0;
    if (response->body) {
        LOG(TRACE, "shttp: body len value %d", response->bodyLen);
        contentLength = (response->bodyLen > 0) ? response->bodyLen : strlen(response->body);
    }
    if (response->bodyCallback) {
        contentLength = (response->bodyLen > 0) ? response->bodyLen : 0;
    }
    if (contentLength > 0) {
        char *tmp = malloc(18 + 8); // max 99 MB
        sprintf(tmp, FSTR("Content-Length: %d\r\n"), contentLength);
        netconn_write(conn, tmp, strlen(tmp), NETCONN_COPY);
        free(tmp);
    }

    LOG(TRACE, "shttp: content length: %d", contentLength);

    // finish header block
    netconn_write(conn, FSTR("\r\n"), 2, NETCONN_NOCOPY);

    // send body
    if (response->body) {
        // body data available, direct send
        LOG(TRACE, "shttp: sending body data (%s)", response->body);
        netconn_write(conn, response->body, contentLength, NETCONN_COPY);
        if (response->free_body) {
            free(response->body);
        }
    }
    if (response->bodyCallback) {
        // callback option, repeatedly call callback and stream out data
        LOG(TRACE, "shttp: sending streaming body data");

        uint32_t position = 0;
        uint32_t chunkLen = 0;
        while (1) {
            char *chunk = response->bodyCallback(position, &chunkLen, response->callbackUserData);
            if (!chunk) {
                // if chunk is NULL, callback is finished, clean up
                LOG(TRACE, "shttp: body chunk stream finished");
                response->cleanupCallback(response->callbackUserData);
                break;
            }
            LOG(TRACE, "shttp: body chunk %d bytes @ %d", chunkLen, position);

            // send the chunk and free the memory
            err = netconn_write(conn, chunk, chunkLen, NETCONN_COPY);
            free(chunk);

            // increment position
            position += chunkLen;

            if (err != ERR_OK) {
                return; // Network disconnected, cancel sending data
            }
        }
    }
}

//
// API
//

ICACHE_FLASH_ATTR void shttp_response_add_headers(shttpResponse *response, ...) {
    char *name, *value;
    char *cName[10], *cValue[10];
    uint8_t headerCount = 0;
    va_list ap;

    // count number of headers
    va_start(ap, response);
    name = va_arg(ap, char *);
    while(name) {
        value = va_arg(ap, char *);
        if (value == NULL) {
            va_end(ap);
            return; // count not divisible by 2? bail out!
        }

        // copy header name and value into struct
        uint16_t len = strlen(name);
        cName[headerCount] = malloc(len + 1);
        memcpy(cName[headerCount], name, len);
        cName[headerCount][len] = '\0';

        len = strlen(value);
        cValue[headerCount] = malloc(len + 1);
        memcpy(cValue[headerCount], value, len);
        cValue[headerCount][len] = '\0';

        LOG(TRACE, "shttp: adding header '%s: %s'", name, value);

        // next
        headerCount++;
        if (headerCount > 9) {
            LOG(DEBUG, "shttp: too many headers!");
            break;
        }
        name = va_arg(ap, char *);
    }
    va_end(ap);

    // no headers, nothing to do
    if (headerCount == 0) {
        LOG(TRACE, "shttp: header count is zero?");
        return;
    }

    // allocate memory
    
    response->headers = malloc((headerCount + 1) * sizeof(shttpHeader));
    if (!response->headers) {
        return; // Out of memory
    }
    for(uint8_t i = 0; i < headerCount; i++) {
        response->headers[i].name = cName[i];
        response->headers[i].value = cValue[i];
    }
    response->headerCount = headerCount;
}

ICACHE_FLASH_ATTR shttpResponse *shttp_empty_response(shttpStatusCode status) {
    shttpResponse *response = malloc(sizeof(shttpResponse));
    memset(response, 0, sizeof(shttpResponse));

    response->responseCode = status;
    return response;
}

#if SHTTP_CJSON
ICACHE_FLASH_ATTR shttpResponse *shttp_json_response(shttpStatusCode status, cJSON *json) {
    shttpResponse *response = shttp_empty_response(status);
    shttp_response_add_headers(response, FSTR("Content-Type"), FSTR("application/json"), NULL);
    response->body = cJSON_Print(json);
    response->free_body = true;
    cJSON_Delete(json);
    return response;
}
#endif /* SHTTP_CJSON */

ICACHE_FLASH_ATTR shttpResponse *shttp_html_response(shttpStatusCode status, char *html, bool autofree) {
    shttpResponse *response = shttp_empty_response(status);
    shttp_response_add_headers(response, FSTR("Content-Type"), FSTR("text/html"), NULL);
    response->body = html;
    response->free_body = autofree;

    return response;
}

ICACHE_FLASH_ATTR shttpResponse *shttp_text_response(shttpStatusCode status, char *text, bool autofree) {
    shttpResponse *response = shttp_empty_response(status);
    shttp_response_add_headers(response, FSTR("Content-Type"), FSTR("text/plain"), NULL);
    response->body = text;
    response->free_body = autofree;

    return response;
}

ICACHE_FLASH_ATTR shttpResponse *shttp_download_response(shttpStatusCode status, char *buffer, uint32_t len, char *filename, bool autofree) {
    shttpResponse *response = shttp_empty_response(status);

    // download means an attachment header
    char *disposition = (char *)FSTR("attachment");
    
    if (filename) {
        // if we have a filename add it to the header
        uint8_t len = 23 + strlen(filename) + 1;
        char *tmp = malloc(len);
        sprintf(tmp, FSTR("attachment; filename=\"%s\""), filename);
        disposition = tmp;
    }

    shttp_response_add_headers(response,
        FSTR("Content-Type"), FSTR("application/octet-stream"),
        FSTR("Content-Disposition"), disposition,
        NULL);

    // response body needs length as it is probably binary
    response->body = buffer;
    response->bodyLen = len;
    response->free_body = autofree;

    // free disposition if constructed
    if (filename) {
        free(disposition);
    }

    return response;

}

ICACHE_FLASH_ATTR shttpResponse *shttp_download_callback_response(shttpStatusCode status, uint32_t len, char *filename, shttpBodyCallback *callback, void *userData, shttpCleanupCallback *cleanup) {
    shttpResponse *response = shttp_download_response(status, NULL, 0, filename, false);
    response->bodyCallback = callback;
    response->callbackUserData = userData;
    response->cleanupCallback = cleanup;

    return response;
}
