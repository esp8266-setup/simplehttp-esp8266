#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "debug.h"
#include "simplehttp/http.h"

#include "parser.h"
#include "router.h"

static struct netconn *listeningConn;
static xQueueHandle connectionQueue;
static xTaskHandle dataTask;

volatile shttpConfig *shttpServerConfig;

ICACHE_FLASH_ATTR static bool bind_and_listen(uint16_t port) {
    listeningConn = netconn_new(NETCONN_TCP);
    if (listeningConn == NULL) {
        LOG(ERROR, "shttp: creating new netconn failed");
        return false;
    }

    netconn_bind(listeningConn, NULL, port);
    netconn_listen(listeningConn);

    // we are listening
    return true;
}

void readTask(void *userData) {
    struct netconn *conn;
    struct netbuf *inbuf = NULL;
    char *recv_buffer;
    uint16_t buflen;
    err_t err;
    shttpParserState *parser;

    while(1) {
        // fetch a connection from the queue
        xQueueReceive(connectionQueue, &conn, portMAX_DELAY);

        // create a parser
        parser = shttp_parser_init_state();

        // receive data
        while(1) {
            err = netconn_recv(conn, &inbuf);

            if (err != ERR_OK) {
                LOG(DEBUG, "shttp: client disconnected");
                break;
            } else {
                netbuf_data(inbuf, (void **)&recv_buffer, &buflen);

                // received some bytes, run parser on it
                if (!shttp_parse(parser, recv_buffer, buflen, conn)) {
                    // parser thinks we should close the connection
                    LOG(DEBUG, "shttp: parse called for quit");
                    break;
                }
            }
        }

        // clean up
        netconn_close(conn);
        netbuf_delete(inbuf);
        netconn_delete(conn);
        shttp_destroy_parser(parser);
        LOG(DEBUG, "shttp: connection closed");
    }
}

ICACHE_FLASH_ATTR void shttp_listen(shttpConfig *config) {
    struct netconn *incoming;
    err_t err;

    // bind and listen
    bool result = bind_and_listen(config->port);
    if (result == false){
        LOG(ERROR, "shttp: Giving up");
        return;
    }

    // Create data processing queue
    connectionQueue = xQueueCreate(SHTTP_MAX_QUEUED_CONNECTIONS, sizeof(int));
    if (connectionQueue == NULL) {
        LOG(ERROR, "shttp: Could not create connection queue, terminating");
        netconn_close(listeningConn);
        netconn_delete(listeningConn);
        return;
    }

    // start data processing task
    if (xTaskCreate(readTask, "shttp.read", SHTTP_STACK_SIZE, NULL, SHTTP_PRIO, &dataTask) != pdPASS) {
        LOG(ERROR, "shttp: Could not create data processing task, terminating");
        vQueueDelete(connectionQueue);
        netconn_close(listeningConn);
        netconn_delete(listeningConn);
        return;
    }

    shttpServerConfig = config;
    LOG(DEBUG, "shttp: server ready to accept connections");

    // accept connections
    while(1) {

        // this blocks until a client connects
        err = netconn_accept(listeningConn, &incoming);
        if (err == ERR_OK) {
            LOG(TRACE, "shttp: Client connected, signaling communications thread");
            xQueueSendToBack(connectionQueue, &incoming, portMAX_DELAY);
        } else {
            LOG(ERROR, "shttp: Could not accept connection, terminating");
            vTaskDelete(dataTask);
            vQueueDelete(connectionQueue);
            netconn_close(listeningConn);
            netconn_delete(listeningConn);
        }
    }
}
