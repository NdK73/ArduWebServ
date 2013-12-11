#include "confread.h"
#include "string.h"
#include "httpd.h"
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include <Wire.h>

// DHCP uses about 3K !
#define DHCP_SUPPORTED 0
// Pin used for selectiong Ethernet ctrl
#define ETH_SS 10
// Pin used for selecting SD card
#define SD_SS 4

// minimum time an input have to remain stable to be accepted (msec)
#define BOUNCE_TIME 20

static const byte INPUTS[]={2,3,5,6,7,8,9};
static const byte ANALOGS[]={A0, A1, A2, A3};  // A4 and A5 are reserved for I2C !

// Define POST/GET variables that will be accepted
PROGMEM const char rb_rel1[] = "rel1";
PROGMEM const char rb_rel2[] = "rel2";
PROGMEM const char rb_rel3[] = "rel3";
PROGMEM const char rb_rel4[] = "rel4";
PROGMEM const char rb_rel5[] = "rel5";
PROGMEM const char rb_rel6[] = "rel6";
PROGMEM const char rb_vout[] = "vout";
PROGMEM const char pass[] = "pass";

// Add vars to array used by httpd.cpp
PROGMEM const char * http_parameters[] = {
  rb_rel1,
  rb_rel2,
  rb_rel3,
  rb_rel4,
  rb_rel5,
  rb_rel6,
  rb_vout,
//  pass,
};

// Max len of a var value
#define MAX_VAL_LEN 8

// Passwords for commands (limited to MAX_VAL_LEN)
PROGMEM const char pass1[] = "scritto";
PROGMEM const char pass2[] = "da NdK";
PROGMEM const char pass3[] = "reset";
PROGMEM const char *passwords[] = {
  pass1,
  pass2,
  pass3,
};

static int auth=ALEN(passwords);  // Auth state; if ALEN(passwords) then no auth given yet

/*********** Relay Board definitions *************/
// Up to 6 inputs are mapped to relays on relay board (there must be at least this many inputs!)
#define N_RELS 7
#define RB_ADDR (0x27)
#define RB_IN 0
#define RB_OUT 1
#define RB_IINV 2
#define RB_DIR 3
static byte rb_state = 0;


byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// ip represents the fixed IP address to use if DHCP is disabled.
byte ip[] =    {  0,  0,  0, 0};
byte netmask[]={255,255,255, 0};
byte gateway[]={  0,  0,  0, 0};
byte dns[]=    {  0,  0,  0, 0};

EthernetServer server(80);

static bool inputs[sizeof(INPUTS)];
static long lastchange[sizeof(INPUTS)];  // debounce logic timers

// Callbacks and helper functions
static void processget(int var, Stream & val);
static void processpost(int var, Stream & val);
static const char *replacevars(const char *name, URI &page);
static void process_inputs();
static void i2ccmd(byte addr, byte reg, byte val);  // write val to addr:reg

void setup()
{
  Wire.begin();  // Init I2C as master (on pins A4 and A5)
  i2ccmd(RB_ADDR, RB_OUT, 0x80);  // All OFF
  i2ccmd(RB_ADDR, RB_DIR, 0);     // All outputs
  Serial.begin(9600); // DEBUG
  pinMode(ETH_SS, OUTPUT); // Ethernet shield
  pinMode(SD_SS, OUTPUT); // SD card
  for(byte cnt=0; cnt<sizeof(INPUTS); ++cnt) {
    pinMode(INPUTS[cnt], INPUT);
    digitalWrite(INPUTS[cnt], HIGH);       // turn on pullup resistors
  }

  if (!SD.begin(SD_SS)) {
    Serial.println("SD init failed!");
    lockup();
  }

/*
  if (!SD.exists("index.htm")) {
    Serial.println(F("ERROR - Can't find index.htm file!"));
    lockup();
  }
*/

  // Read config.txt from card
  readConfig();
  
  if(!(ip[0]|ip[1]|ip[2]|ip[3])) { // if IP is 0.0.0.0 then use DHCP
#if(DHCP_SUPPORTED)
    Serial.println(F("Attempting DHCP"));
    Ethernet.begin(mac);
#else
    Serial.println(F("No IP in config.txt"));
    lockup();
#endif
  } else {
    Ethernet.begin(mac, ip, dns, gateway, netmask);
  }

  server.begin();
}

long m;  // Avoid a function call to millis()

void loop() 
{
  m=millis();

  // Read & store input states so variables substitution is coherent
  for(byte cnt=0; cnt<sizeof(INPUTS); ++cnt) {
    bool i=!digitalRead(INPUTS[cnt]);  // Inputs are active LOW
    if(i!=inputs[cnt]) {
      if(m-lastchange[cnt]>BOUNCE_TIME) {
        inputs[cnt]=i;      // Ok, input changed to the new state for long enough
        lastchange[cnt]=m;
      }
    } else
      lastchange[cnt]=m; // Reset input debouncer: input is still in the same state
  }

  // Process inputs.
  // Note: here auth is still set from previous process_http() call, so it could be used.
  process_inputs();

  auth=ALEN(passwords);  // Forget previous authorization
  // This processes eventual commands sent throught http
//  process_http(server, processget, http_parameters, ALEN(http_parameters), processpost, http_parameters, ALEN(http_parameters), replacevars);
  process_http(server, NULL, NULL, 0, process_cmd, http_parameters, ALEN(http_parameters), replacevars);
}

static void i2ccmd(byte addr, byte reg, byte val)
{
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static void process_inputs()
{
  static long pulser=0;
  static byte oldstate=0;
  byte state=0, outs=0, mask=1, invmask=0;

  // Up to N_RELS inputs are mapped to relays
  for(byte cnt=0; cnt<N_RELS; ++cnt) {
    if(inputs[cnt]) {
      state|=mask;
    }
    mask<<=1;
  }
  // Pulse status LED
  if(m-pulser>500) {  // 1Hz
    state|=0x80;  // Invert status LED
    pulser=m;
  }

  // Do rising edge-detection: if a bit was 0 and becomes 1, toggle relay
  invmask=(~oldstate)&state;

  if(invmask) { // at least an input changed
    rb_state^=invmask; // every newly active input toggles corresponding relay
    i2ccmd(RB_ADDR, RB_OUT, rb_state);
  }

  // Save current inputs state
  oldstate=state;
}

// this gets called only for *known* commands in GET/POST
static void process_cmd(int var, Stream & val)
{
  static PROGMEM const char on[]="on";
  static PROGMEM const char off[] = "off";
  static PROGMEM const char *onoff[]= {off, on};

  static char value[MAX_VAL_LEN+1];

  byte t;

  // Get value of var
  t=tokenize(val, "& \n", value, MAX_VAL_LEN+1);
  if(!t) { // Value is longer than allowed
    consume_to(val, "& \n");
  }
/*
  Serial.print(F(" #"));
  Serial.print(var);
  Serial.print(F("="));
  Serial.println(value);
*/
  switch(var) {
    case 0:  // rel1
    case 1:  // rel2
    case 2:  // rel3
    case 3:  // rel4
    case 4:  // rel5
    case 5:  // rel6
    case 6:  // vout
      t=(byte)lookup(value, onoff, ALEN(onoff));
      if(ALEN(onoff)==t)
        break;  // Ignore value if different from on or off
      // Altera i bit di rb_state ed invia il comando
      if(t)
        bitSet(rb_state, var);
      else
        bitClear(rb_state, var);
      i2ccmd(RB_ADDR, RB_OUT, rb_state);
      break;
//    case 7:
//      break;
//    default:  // Clean value of unknown vars
  }
}
/*
static void processget(int var, Stream & val)
{
  Serial.println(F("Processing GET vars:"));
  process_cmd(var, val);
}

static void processpost(int var, Stream & val)
{
  Serial.println(F("Processing POST vars:"));
  process_cmd(var, val);
}
*/
static const char *replacevars(const char *name, URI &page)
{
  static char buff[32];  // Temp buff to return "complex" results

  if(!strncmp_P(name, PSTR("input"), 5)) {
    int in=atoi(name+5)-1;
    if(0<=in && in<sizeof(INPUTS)) {
        return inputs[in]?"on":"off";
    } else {
      return "--";
    }
  }
  else if(!strncmp_P(name, PSTR("analog"), 5)) {
    int in=atoi(name+6)-1;
    if(0<=in && in<sizeof(ANALOGS)) {
/*
Serial.print(F("Reading A"));
Serial.print(in, DEC);
Serial.print(F(" = pin "));
Serial.println(ANALOGS[in]);
*/
      itoa(analogRead(ANALOGS[in]), buff, 10);
      return buff;
    } else {
      return "--";
    }
  }
  else if(!strncmp_P(name, PSTR("relay"), 5)) {
    int relay=atoi(name+5)-1;
    if(relay<N_RELS) {
      return bitRead(rb_state, relay)?"on":"off";
    } else
      return "--";
  }
  else if(!strncmp_P(name, PSTR("nrelay"), 5)) {
    int relay=atoi(name+6)-1;
    if(relay<N_RELS) {
      return (!bitRead(rb_state, relay))?"on":"off";
    } else
      return "--";
  }
  else if(!strcmp_P(name, PSTR("title")))
    strcpy_P(buff, PSTR("Centralina campo"));
//  else if(!strcmp_P(name, "next")) {
//    return "1";
//  }
  else {
    buff[0]='%';
    strncpy(buff+1, name, sizeof(buff)-2);
    strncat(buff, "%", sizeof(buff)-1);
  }
  return buff;
}

