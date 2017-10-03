#ifndef shttp_router_h_included
#define shttp_router_h_included

#include "simplehttp/http.h"

void shttp_exec_route(char *path, shttpMethod method, shttpRequest *request, struct netconn *conn);

#endif /* shttp_router_h_included */