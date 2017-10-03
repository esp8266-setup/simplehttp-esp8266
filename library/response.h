#ifndef shttp_response_h_included
#define shttp_response_h_included

#include "simplehttp/http.h"

#include <lwip/opt.h>
#include <lwip/arch.h>
#include <lwip/api.h>

void shttp_write_response(shttpResponse *response, struct netconn *conn);

#endif /* shttp_response_h_included */