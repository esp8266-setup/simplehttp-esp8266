#include "simplehttp/http.h"

// ATTENTION: for all following functions
// data will be modified and the response pointers will directly point,
// to parts of this data to conserve memory.
// do not free data until request can be completely discarded!

void parse_introduction(char *data, char **path, shttpMethod *method, shttpParameter **parameters, uint8_t *numParameters);
shttpHeader *parse_header(char *data);

