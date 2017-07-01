# simple http server for esp8266

This library implements a very simple HTTP application server (API server).

## Documentation

See the header file `include/simplehttp/http.h` for documentation.

## Example code

```c
#include <simplehttp/http.h>
#define BUFF_SZ 200

// greet user by name, name is the last part of the request URL
static shttpResponse *helloName(shttpRequest *request) {
    // check if we have a parameter, this should be made sure by the lib,
    // but better safe than sorry
    if (request->numPathParameters == 1) {

        // create greeting message
        char *responseBuffer = malloc(BUFF_SZ);
        int len = snprintf(
            responseBuffer, BUFF_SZ,
            "Hello %s!", request->pathParameters[0]
        );

        // return plain text response
        return shttp_text_response(shttpStatusOk, responseBuffer);
    } else {
        // no parameter, bad request, no treats for you!
        return BAD_REQUEST;
    }
}

// return simple greeting without name
static shttpResponse *helloUnknown(shttpRequest *request) {
    return shttp_text_response(shttpStatusOk, "Hello you!");
}

// just a demo how to return a custom response for a wildcard
static shttpResponse *custom404(shttpRequest *request) {
    // just return an empty response with the code 404
    return NOT_FOUND;
}

void main(void) {
    shttpConfig config;

    // set a hostname, if a request with a different host header
    // arrives the server will automatically return 404
    config.hostname = "esp8266";

    // the port to use, default should be 80
    config.port = "80";

    // we don't care if the url ends with a slash
    config.appendSlashes = 1;

    // now define three routes
    config.routes = (shttpRoute *[]){
        // first route has a parameter
        GET("/hello/?", helloName),

        // second route is essentially the same as the first but will
        // only be called if there is no parameter
        GET("/hello", helloUnknown),

        // this route is a catchall and just returns 404
        GET("*", custom404),

        // we're finished, do not forget the sentinel
        NULL
    };

    // start the server, this never returns
    shttp_listen(&config);
}
```

## Building

Some pointers:

1. Get the ESP8266 compiler: https://github.com/pfalcon/esp-open-sdk
2. Get the SDK
3. Unpack SDK somewhere
4. Set up the environment
    ```bash
    export PATH="/path/to/compiler/xtensa-lx106-elf/bin:$PATH"
    export SDK_PATH="$HOME/esp-sdk/ESP8266_RTOS_SDK"
    export BIN_PATH="$HOME/esp-sdk/build"

    export CFLAGS="-I${SDK_PATH}/include -I${SDK_PATH}/extra_include $CFLAGS"
    export LDFLAGS="-L${SDK_PATH}/lib $LDFLAGS"
    ```
5. Switch to the base directory and run `make`
6. Grab the library from `.output/lib/libsimplehttp.a`
7. Grab the headers from `include/*.h`

## Legal

License: 3 Clause BSD (see LICENSE-BSD.txt)