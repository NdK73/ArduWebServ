#include "string.h"
#include <Ethernet.h>

typedef enum MethodType {
  MethodUnknown,
  MethodGet,
  MethodPost,
  MethodHead
};

#define MAX_URI_LEN 31
typedef char URI[MAX_URI_LEN+1];  // SD card only supports 8.3 names -- leave space for a subdirectory

// Max chars to use for a var name (8 "should be enough for everyone" :) )
#define MAX_VAR_LEN 8

#define ALEN(arr) (sizeof(arr)/sizeof(arr[0]))

// Callback function to be used when sending pages.
// Every time a %name% is found, the replacer function gets called to perform variable substitution
typedef const char * (*replacer)(const char *name, URI &page);
// Callback function to parse URI parameters and POST variables
// Parsing *only* the parameter of the current var is responsibility of parsepars function!
// After call, next Stream.read() must return an arg separator ('&', '\n' or NUL)
typedef void (*parsepars)(int var, Stream & val);

// Process HTTP request -- to be called from loop()
void process_http(EthernetServer &, parsepars par, const PROGMEM char *[], int, parsepars post, const PROGMEM char *[], int, replacer cbk);

// URL-Decode string s in place
void urldecode(char *s);

