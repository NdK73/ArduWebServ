#ifndef STRING_H
#define STRING_H
#include <SD.h>

int parseint(Stream &f);
int parsehex(Stream &f);
byte parseip(Stream &f, byte addr[4]);

void nextline(Stream &f);  // Go to next line
// Read a token from stream, putting it in out.
// Max token len can be olen, including terminating NUL 
// returns the first separator found, or NUL if not found
char tokenize(Stream &f, const char * sep, char *out, byte olen);

void consume(Stream &f, const char *sep);
void consume_to(Stream &f, const char *sep);

// Tries to find str between first entries of table array
// Returns index of found string or entries if not found
int lookup(const char *str, PROGMEM const char **table, const int entries);

#endif

