#include "string.h"

// Reads an integer decimal value from file f, starting at current position
int parseint(Stream &f)
{
  int t=0;
  while(f.available()) {
    byte b= f.peek();
    if(b>='0' && b<='9') {
      t=10*t+(b-'0');
      f.read(); // Consume used (valid) character
    } else
      break;
  }

  return t;
}

// Reads an integer hex value from file f, starting at current position
int parsehex(Stream &f)
{
  int t=0;
  while(f.available()) {
    byte b=f.peek();
    if(b>='0' && b<='9')
      t=16*t+(b-'0');
    else if(b>='a' && b<='f')
      t=16*t+(b-'a'+10);
    else if(b>='A' && b<='F')
      t=16*t+(b-'A'+10);
    else
      break;
    f.read(); // Consume used (valid) character
  }

  return t;
}

// Reads an IP address from f, starting at current position.
// Returns 0 if parsing was OK.
byte parseip(Stream &f, byte addr[4])
{
  int tmp, cnt;
  for(cnt=0; cnt<4; ++cnt) {
    tmp=parseint(f);
    addr[cnt]=tmp&0xff;

    // Skip over dots, checking syntax
    if(cnt<3 && f.peek() != '.')
      break;
    else
      f.read(); // Skip over '.'
  }

  if(cnt<4) return (byte)1;
  return (byte)0;
}

// Goes to start of next line in file f
void nextline(Stream &f)
{
  while(f.available()) {
    byte c=f.read();
    if('\n'==c) break;
  }
  if('\r'==f.peek())
    f.read();
}

// Copies from f to out till a separator is found or olen is reached.
// olen *includes* the terminating NUL, that's *always* included unless olen is 0.
// Returns true if a separator was found
// Consumes the terminal sequence of separators.
char tokenize(Stream &f, const char * sep, char *out, byte olen)
{
  char rv=0;
  if(!olen) return 0;  // No space available: no token found

//Serial.print(" tokenizing ");
  while(olen>1 && f.available()) {
    if(NULL==strchr(sep, f.peek())) {
//Serial.write(f.peek());
      *out++=f.read();
      --olen;
    } else {
      rv=f.read();
      break;  // At first separator, stop copying and return it
    }
  }
  *out=0; // Terminate string

//Serial.write('\n');
/*
  while(f.available() && strchr(sep, f.peek()))
    f.read();
*/
  return rv;
}

// Consumes all consecutive chars in input stream that are contained in sep
void consume(Stream &f, const char *sep)
{
//Serial.print(F("Consume: "));
  while(f.available() && strchr(sep, f.peek()))
//Serial.write(
    f.read()
//)
    ;
}

// Consumes all consecutive chars in input stream that are NOT contained in sep
void consume_to(Stream &f, const char *sep)
{
//Serial.print(F("Consume: "));
  while(f.available() && !strchr(sep, f.peek()))
//Serial.write(
    f.read()
//)
    ;
}

// Tries to find str between first entries of table array
// Returns index of found string or entries if not found
int lookup(const char *str, PROGMEM const char **table, const int entries) {
  int cnt=0;
  while(cnt<entries && strcmp_P(str, (char*)pgm_read_word(&(table[cnt])))) {
    ++cnt;
  }

  return cnt; 
}

