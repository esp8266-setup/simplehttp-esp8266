#include "simplehttp/http.h"

// ATTENTION: for all following functions
// data will be modified and the response pointers will directly point,
// to parts of this data to conserve memory.
// do not free data until request can be completely discarded!

typedef struct _shttpParserState shttpParserState;

shttpParserState *parser_init_state(void);
bool parse(shttpParserState *state, char *buffer, uint16_t len);
void destroy_parser(shttpParserState *state);