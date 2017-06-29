#ifndef shttp_http_h_included
#define shttp_http_h_included

// simple key/value struct, re-used for headers and parameters
typedef struct _shttpKeyValue {
    char *name;
    char *value;
} shttpKeyValue;

// a HTTP header
typedef shttpKeyValue shttpHeader;

// a HTTP URL parameter
typedef shttpKeyValue shttpParameter;

// HTTP request data
typedef struct _shttpRequest {
    // headers on the HTTP request
    shttpHeader *headers;
    // number of headers
    uint8_t numHeaders;

    // URL parameters
    shttpParameter *parameter;
    // number of parameters
    uint8_t numParameters;
} shttpRequest;

// HTTP status code to make code more readable
typedef enum _shttpStatusCode {
    shttpStatusOK = 200,
    shttpStatusCreated = 201,
    shttpStatusAccepted = 202,
    shttpStatusNoContent = 204,
    
    shttpStatusMovedPermanently = 301,
    shttpStatusFound = 302,
    shttpStatusNotModified = 304,

    shttpStatusBadRequest = 400,
    shttpStatusUnauthorized = 401,
    shttpStatusForbidden = 403,
    shttpStatusNotFound = 404,
    shttpStatusNotAcceptable = 406,
    shttpStatusConflict = 409,
    shttpStatusRequestURITooLong = 414,

    shttpStatusInternalError = 500,
    shttpStatusNotImplemented = 501,
    shttpStatusBadGateway = 502,
    shttpStatusServiceUnavailable = 503
} shttpStatusCode;

// data generator callback
// parameters are:
// - the number of bytes already sent
// - output parameter (length of the chunk returned)
// - user data pointer from above
// returns char pointer with new data or NULL to finish the request (closes the connection)
typedef char *(*shttpBodyCallback)(uint32_t sentBytes, uint32_t *len, void *userData);

// cleanup callback, called to clean up user data pointer
typedef void *(shttpCleanupCallback)(void *userData);

// HTTP response, returned from route callback
typedef struct _shttpResponse {
    // response code
    shttpStatusCode responseCode;

    // Headers to set, close up with NULL sentinel
    // Content-Length is calculated automatically
    shttpHeader *headers;

    // the body to return, set to NULL to define callback
    char *body;
    // body length,
    // - set to zero to use zero terminated string in body
    // - if set to zero using the callback, the connection is
    //   terminated when the response finishes
    uint32_t bodyLen;

    // user data pointer given to body callback
    void *callbackUserData;

    // body callback
    shttpBodyCallback bodyCallback;

    // cleanup callback
    // is called whenever the response is finished (either erroring out
    // or finishing successfully) to clean up the user data pointer
    shttpCleanupCallback cleanupCallback;
} shttpResponse;


// HTTP method flags
typedef enum _shttpMethod {
    shttpMethodGET     = (1 << 0),
    shttpMethodPOST    = (1 << 1),
    shttpMethodPUT     = (1 << 2),
    shttpMethodPATCH   = (1 << 3),
    shttpMethodDELETE  = (1 << 4),
    shttpMethodOPTIONS = (1 << 5),
    shttpMethodHEAD    = (1 << 6),
} shttpMethod;

typedef struct _shttpRoute {
    // allowed methods for this route, add them together to allow
    // multiple methods (flags)
    shttpMethod allowedMethods;

    // path to match
    // - use ? for parameter, parameters may not include slashes
    // - use * for wildcard. Wildcards are only allowed at the end
    char *path;

    // callback to call when route found
    shttpResponse *(*callback)(shttpRequest *request);

    // if you define multiple routes with the same path and different
    // allowedMethods then the list is processed until a matching
    // entry is found.
} shttpRoute;

typedef struct _shttpConfig {
    // Hostname of the device,
    // set to NULL to listen to everything
    char *hostName; 

    // Port to listen to. You can probably use anything here
    uint16_t port;

    // set to 1 if a slash at the end of the url should be ignored
    // If true it essentially means the following is equivalent:
    // - `GET /hello`
    // - `GET /hello/`
    bool appendSlashes;

    // defined routes (for callbacks), close with a NULL sentinel
    // be aware that comparing the list is done sequentially, if no
    // match could be found the next item is tried until we reach the
    // end of the list and the server returns a 404
    shttpRoute *routes;
} shttpConfig;

// Start the shttp server, this function does not return
// use it in a thread or RTOS task.
void shttp_listen(shttpConfig *config);

// URL encode value, caller has to free the result
char *shttp_url_encode(char *value);

// URL decode value, caller has to free the result
char *shttp_url_decode(char *value);

//
// convenience functions
//

shttpResponse *shttp_empty_response(shttpStatusCode status);
#define BAD_REQUEST shttp_empty_response(shttpStatusBadRequest)
#define NOT_FOUND shttp_empty_response(shttpStatusNotFound)
#define NOT_IMPLEMENTED shttp_empty_response(shttpStatusNotImplemented)
#define NOT_ALLOWED shttp_empty_response(shttpStatusNotAllowed)
#define UNAUTHORIZED shttp_empty_response(shttpStatusUnauthorized)

// return a json response with correct headers set
shttpResponse *shttp_json_response(shttpStatusCode status, cJSON *json);

// return a html response with correct headers set, NULL terminated
shttpResponse *shttp_html_response(shttpStatusCode status, char *html);

// return a text response with correct headers set, NULL terminated
shttpResponse *shttp_text_response(shttpStatusCode status, char *text);

// return a download with correct headers set and a specific length
// set len to 0 to indicate a NULL terminated string and calculate automatically
shttpResponse *shttp_download_response(shttpStatusCode status, uint32_t len, char *buffer);

// return a download with the callback interface to conserve memory
// if len is set to 0 the connection will be terminated after finishing
shttpResponse *shttp_download_callback_response(shttpStatusCode status, uint32_t len, shttpBodyCallback callback, void *userData, shttpCleanupCallback cleanup);

#endif /* shttp_http_h_included */