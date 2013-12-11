#include "confread.h"
#include "string.h"
#include <SD.h>

extern byte mac[6], ip[4], netmask[4], gateway[4], dns[4];

void readConfig()
{
  byte err=0;
  File f = SD.open("config.txt");
  while(!err && f.available()) {
    byte c=f.read();
    switch(c) {
      case '\n':
      case '\r':
      case ' ':
      case '\t':
        break;  // Ignore whitespace
      case '#':
        nextline(f); // Ignore comment lines
        break;
      case 'h':  // HW address (MAC)
        mac[0]=parsehex(f);
        f.read();
        mac[1]=parsehex(f);
        f.read();
        mac[2]=parsehex(f);
        f.read();
        mac[3]=parsehex(f);
        f.read();
        mac[4]=parsehex(f);
        f.read();
        mac[5]=parsehex(f);
        //nextline(f); // Don't require a new line
        break;
      case 'i':  // IP address
        err=parseip(f, ip);
        //nextline(f); // Don't require a new line
        break;
      case 'm':  // MASK
        err=parseip(f, netmask);
        //nextline(f); // Don't require a new line
        break;
      case 'g':  // GW
        err=parseip(f, gateway);
        //nextline(f); // Don't require a new line
        break;
      case 'd':  // DNS
        err=parseip(f, dns);
        //nextline(f); // Don't require a new line
        break;
      default:
        Serial.print(F("Unexpected char: "));
        Serial.println(c);
    }
  }
}

/* Halts after an error */
void lockup() {
  while(true) delay(1000);
}

