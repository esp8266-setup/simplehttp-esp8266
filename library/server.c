#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <time.h>

#include "simplehttp/http.h"

#include "parser.h"
#include "router.h"

typedef struct _connection {
    // data chunks we have to free when closing this connection
    uint8_t numChunks;
    char *chunks;

} connection;

void shttp_listen(shttpConfig *config) {

}
