/*
 * Web Server - multi-page.  5 Feb 2011.
 * Need an Ethernet Shield over Arduino.
 *
 * By NdK (ndk.clanbo(at)gmail(dot)com) extending/rewriting the good work of
 * Martyn Woerner extending the good work of
 * Alessandro Calzavara, alessandro(dot)calzavara(at)gmail(dot)com
 * and Alberto Capponi, bebbo(at)fast-labs net
 * for Arduino community! :-)
 * 
 */

#include "httpd.h"
#include <SD.h>

#define DELIMITERS " \n\r"
#define WEBROOT "/web"
#define WEBROOT_LEN 4

static MethodType readHttpRequest(EthernetClient &client, URI &file, parsepars params, const PROGMEM char *getvars[], int getvarslen, parsepars postparams, const PROGMEM char *postvars[], int postvarslen);
static MethodType readRequestLine(EthernetClient &client, URI &uri, parsepars cbk, const PROGMEM char *[], int);
static bool readRequestHeader(EthernetClient &, int&);
static void send404(EthernetClient & client, URI & uri, int offset);
static void sendPage(EthernetClient & client, URI & uri, MethodType m, replacer cbk);
static void processargs(EthernetClient &c, parsepars f, const PROGMEM char *vars[], int nvars);

static PROGMEM const char sMethodGet[] = "GET";
static PROGMEM const char sMethodPost[] = "POST";
static PROGMEM const char sMethodHead[] = "HEAD";
static PROGMEM const char *aMethods[] = {sMethodGet, sMethodPost, sMethodHead};

/*****
 * Called from loop() to process incoming http requests
 *****/
void process_http(EthernetServer & server, parsepars ppar, const PROGMEM char *getvars[], int getvarslen, parsepars ppost, const PROGMEM char *postvars[], int postvarslen, replacer rvars)
{
  EthernetClient client = server.available();

  if (client)
  {
    // now client is connected to arduino we need to extract the
    // following fields from the HTTP request.
    URI uri;              // filename of requested resource
    uri[0]=0;
    MethodType eMethod = readHttpRequest(client, uri, ppar, getvars, getvarslen, ppost, postvars, postvarslen);

    if(0==strcmp(uri, "/"))
      strcpy(uri, "/index.htm");

    // Sanitize file name and make sure it's in /web
    // Should do more checks...
    byte len=strlen(uri);
    if(len>MAX_URI_LEN-WEBROOT_LEN) {
      send404(client, uri, 0);
    } else {
      // In-place move requested URI WEBROOT_LEN chars forward
      memmove(uri+WEBROOT_LEN, uri, strlen(uri)+1);
      memcpy(uri, WEBROOT, WEBROOT_LEN);

//Serial.print(F("Using URI "));
//Serial.println(uri);

      if(SD.exists(uri)) {  // Send only existing files
        sendPage(client, uri, eMethod, rvars);
      } else {
        send404(client, uri, WEBROOT_LEN);
      }
    }

    // give the web browser time to receive the data
    delay(1);
    client.stop();
  }
}

/**********************************************************************************************************************
*                                              Method for read HTTP Header Request from web client
*
* The HTTP request format is defined at http://www.w3.org/Protocols/HTTP/1.0/spec.html#Message-Types
* and shows the following structure:
*  Full-Request and Full-Response use the generic message format of RFC 822 [7] for transferring entities. Both messages may include optional header fields
*  (also known as "headers") and an entity body. The entity body is separated from the headers by a null line (i.e., a line with nothing preceding the CRLF).
*      Full-Request   = Request-Line       
*                       *( General-Header 
*                        | Request-Header  
*                        | Entity-Header ) 
*                       CRLF
*                       [ Entity-Body ]    
*
* The Request-Line begins with a method token, followed by the Request-URI and the protocol version, and ending with CRLF. The elements are separated by SP characters.
* No CR or LF are allowed except in the final CRLF sequence.
*      Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
* HTTP header fields, which include General-Header, Request-Header, Response-Header, and Entity-Header fields, follow the same generic format.
* Each header field consists of a name followed immediately by a colon (":"), a single space (SP) character, and the field value.
* Field names are case-insensitive. Header fields can be extended over multiple lines by preceding each extra line with at least one SP or HT, though this is not recommended.     
*      HTTP-header    = field-name ":" [ field-value ] CRLF
***********************************************************************************************************************/
// Read HTTP request, setting Uri Index, the requestContent and returning the method type.
static MethodType readHttpRequest(EthernetClient & c, URI & u, parsepars par, const PROGMEM char *gv[], int gvl, parsepars post, const PROGMEM char *pv[], int pvl)
{
  int nContentLength = 0;

  // Read the first line: Request-Line setting Uri Index and returning the method type.
  MethodType eMethod = readRequestLine(c, u, par, gv, gvl);
  // Read any following, non-empty headers setting content length.
  while(readRequestHeader(c, nContentLength))
    ;
/*
Serial.print(F("Method: "));
Serial.println(eMethod);
Serial.print(F("Len="));
Serial.println(nContentLength);
*/
  if(eMethod==MethodPost && nContentLength)
    processargs(c, post, pv, pvl);

  return eMethod;
}

// Read the first line of the HTTP request, setting Uri Index and returning the method type.
// If it is a GET method then we set params to whatever follows the '?'.
// POST method data is added after the CRLF as EntityBody
static MethodType readRequestLine(EthernetClient & client, URI & uri, parsepars params, const PROGMEM char *gv[], int gvl)
{
  MethodType eMethod;
  char tok=0;  // is uri tokenized?

  // Get first line of request:
  // Request-Line = Method SP Request-URI SP HTTP-Version CRLF
  // URI may optionally comprise the URI of a queryable object a '?' and a query
  // see http://www.ietf.org/rfc/rfc1630.txt

  tokenize(client, DELIMITERS, uri, sizeof(URI));

  eMethod = (MethodType)((1+lookup(uri, aMethods, ALEN(aMethods)))%ALEN(aMethods));

  // Get URI string w/o query
  tok=tokenize(client, "?" DELIMITERS, uri, sizeof(URI));
  if(!tok) {
    // uri is longer than permitted
    consume_to(client, "?" DELIMITERS);
    tok=client.read();
  }

  // Consider the case of empty query ("/index.htm?")
  if('?'==tok && ' '!=client.peek()) { // There's a query string in URI: parse it
    processargs(client, params, gv, gvl);
  }

  // Drop protocol info
  nextline(client);

  return eMethod;
}

// Read a line of header; if a content len is specified, set contentLen
// return false if the last header line have been parsed
static bool readRequestHeader(EthernetClient &client, int& contentLen)
{
  static PROGMEM const char tag[]="Content-Length:";
  
  URI tmp;
  tokenize(client, DELIMITERS, tmp, MAX_URI_LEN);
  if(!tmp[0])  // Empty line
    return false;

  if(!strcmp_P(tmp, tag)) {
    contentLen=parseint(client);
  }
  nextline(client);
  return true;
}

/**********************************************************************************************************************
*                                                              Send Pages
       Full-Response  = Status-Line         
                        *( General-Header   
                         | Response-Header 
                         | Entity-Header ) 
                        CRLF
                        [ Entity-Body ]   

       Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
       General-Header = Date | Pragma
       Response-Header = Location | Server | WWW-Authenticate
       Entity-Header  = Allow | Content-Encoding | Content-Length | Content-Type
                      | Expires | Last-Modified | extension-header
*
***********************************************************************************************************************/
static void send404(EthernetClient & client, URI & uri, int offset)
{
//Serial.print(uri);
//Serial.println(F(" not found!"));
  client.print(F("HTTP/1.1 404 Not Found\nServer: arduino\nContent-Type: text/html\n\n"
      "<html><head><title>Arduino Web Server - Error 404</title></head>"
      "<body><h1>Error 404: Sorry, page '"));
  client.print(uri+offset);  // Don't tell where webroot is...
  client.println(F("' cannot be found!</h1></body>"));
}

static void sendPage(EthernetClient & client, URI &uri, MethodType m, replacer rvars)
{
  char *ext=strrchr(uri, '.');
  const char *imgtype=NULL;

  if(ext) {
    ++ext;
    if(!strcmp("jpg", ext))
      imgtype="jpeg";
    else if(!strcmp("png", ext))
      imgtype="png";
    else if(!strcmp("ico", ext))
      imgtype="x-icon";
  }

  File f = SD.open(uri);

  if(NULL==imgtype) {
    client.print(F("HTTP/1.0 200 OK\nServer: arduino\nCache-Control: no-store, no-cache, must-revalidate\n"
        "Pragma: no-cache\nConnection: close\nContent-Type: text/"));
    if(!strcmp("htm", ext))
      client.println(F("html"));
    else
      client.println(ext);  // All unknown .EXTensions are sent as text/EXT -- in particular css files

    client.println("");  // End of header

    while(f.available()) {
      byte b=f.read();
      if('%'==b && rvars) {
        char var[MAX_VAR_LEN+1];
        if(tokenize(f, "%", var, MAX_VAR_LEN+1))
          client.print(rvars(var, uri));  // Replace %variable% with its value
        else {
          client.write('%');  // Bad variable: just print it as-is
          client.print(var);
        }
      } else {
        client.write(b);
      }
    }
  } else {
    client.print(F("HTTP/1.1 200 OK\nServer: arduino\nContent-Length: "));
    client.println(f.size(), DEC);
    client.print(F("Content-Type: image/"));
    client.println(imgtype);
    client.println("");
    while(f.available()) {
      client.write(f.read());
    }
  }
  f.close();
}

void urldecode(char *s)
{
  char *dst;
  for(dst=s; *s; ++s) {
    if('%'==*s) {
      ++s;
      if(*s>='0' && *s<='9')
        *dst=(*s)-'0';
      else if(*s>='a' && *s<='f')
        *dst=(*s)-'a'+10;
      else if(*s>='A' && *s<='F')
        *dst=(*s)-'A'+10;
      else {
        *dst='?';
        ++dst;
        *dst=0;
        return;  // bail out
      }
      ++s;
      if(*s>='0' && *s<='9')
        *dst=16*(*dst)+((*s)-'0');
      else if(*s>='a' && *s<='f')
        *dst=16*(*dst)+((*s)-'a'+10);
      else if(*s>='A' && *s<='F')
        *dst=16*(*dst)+((*s)-'A'+10);
/*
      else {
        *dst='?';
        ++dst;
        *dst=0;
        return;  // bail out
      }
*/
    }
    else if('+'==*s)
      *dst=' ';
    else
      *dst=*s;
    ++dst;
  }
  *dst=0;
}

static void processargs(EthernetClient &c, parsepars f, const PROGMEM char *v[], int vl)
{
  char var[MAX_VAR_LEN+1];
  char t=0;

  if(!f || !v || !vl)
    consume_to(c, DELIMITERS);  // Can't call a NULL function, or no vars!

//Serial.print(F("Vars: "));
  while(c.available()) {
    char t=tokenize(c, "=&" DELIMITERS, var, MAX_VAR_LEN+1);
    // Beware too long var names! Yet unhandled!

//Serial.println(t, HEX);

    if(!t || ' '==t || '\n'==t)
      break;

    int i=lookup(var, v, vl);
/*
Serial.print(F("Variable: "));
Serial.print(var);
if(i<vl) {
Serial.print(F(" @ index "));
Serial.println(i);
} else {
Serial.println(F(" not allowed"));
}
*/
    if(i<vl)
      f(i, c);
    else {  // Clean up value of unknown vars
      consume_to(c, "& \n");
      consume(c, "& \n");
    }
  }
}

