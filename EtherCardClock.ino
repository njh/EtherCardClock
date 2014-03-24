/*
 * Arduino ENC28J60 EtherCard NTP client demo
 * With extra bits for nanode
 */

#define NANODE
#define DEBUG

#include <avr/pgmspace.h>
#include <EtherCard.h>
#ifdef NANODE
#include <NanodeMAC.h>
#endif

// Jan 1
#define SECS_YR_1900_2000  (3155673600UL)

#ifdef NANODE
static uint8_t mymac[6] = { 0, 0, 0, 0, 0, 0 };
#else
static uint8_t mymac[6] = { 0x54, 0x55, 0x58, 0x10, 0x00, 0x25 };
#endif


#define MAX_NUM_SERVERS 5

// Now define another array in PROGMEM for the above strings
uint8_t ntpServers[MAX_NUM_SERVERS][4];
uint8_t ntpServerCount = 0;
int currentNtpServer = 0;

// Packet buffer, must be big enough for packet and payload
byte Ethernet::buffer[550];
uint8_t clientPort = 123;

#ifdef NANODE
NanodeMAC mac(mymac);
#endif

// The next part is to deal with converting time received from NTP servers
// to a value that can be displayed. This code was taken from somewhere that
// I cant remember. Apologies for no acknowledgement.

uint32_t lastUpdate = 0;
uint32_t timeLong;
// Number of seconds between 1-Jan-1900 and 1-Jan-1970, unix time starts 1970
// and ntp time starts 1900.
#define GETTIMEOFDAY_TO_NTP_OFFSET 2208988800UL

#define	EPOCH_YR	1970
//(24L * 60L * 60L)
#define	SECS_DAY	86400UL
#define	LEAPYEAR(year)	(!((year) % 4) && (((year) % 100) || !((year) % 400)))
#define	YEARSIZE(year)	(LEAPYEAR(year) ? 366 : 365)

static const char day_abbrev[] PROGMEM = "SunMonTueWedThuFriSat";
// isleapyear = 0-1
// month=0-11
// return: how many days a month has
//
// We could do this if ram was no issue:

uint8_t monthlen(uint8_t isleapyear, uint8_t month)
{
  const uint8_t mlen[2][12] = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
  };
  return (mlen[isleapyear][month]);
}


// gmtime -- convert calendar time (sec since 1970) into broken down time
// returns something like Fri 2007-10-19 in day and 01:02:21 in clock
// The return values is the minutes as integer. This way you can update
// the entire display when the minutes have changed and otherwise just
// write current time (clock). That way an LCD display needs complete
// re-write only every minute.
uint8_t gmtime(const uint32_t time, char *day, char *clock)
{
  char dstr[4];
  uint8_t i;
  uint32_t dayclock;
  uint16_t dayno;
  uint16_t tm_year = EPOCH_YR;
  uint8_t tm_sec, tm_min, tm_hour, tm_wday, tm_mon;

  dayclock = time % SECS_DAY;
  dayno = time / SECS_DAY;

  tm_sec = dayclock % 60UL;
  tm_min = (dayclock % 3600UL) / 60;
  tm_hour = dayclock / 3600UL;
  tm_wday = (dayno + 4) % 7;  /* day 0 was a thursday */
  while (dayno >= YEARSIZE(tm_year)) {
    dayno -= YEARSIZE(tm_year);
    tm_year++;
  }
  tm_mon = 0;
  while (dayno >= monthlen(LEAPYEAR(tm_year), tm_mon)) {
    dayno -= monthlen(LEAPYEAR(tm_year), tm_mon);
    tm_mon++;
  }
  i = 0;
  while (i < 3) {
    dstr[i] = pgm_read_byte(&(day_abbrev[tm_wday * 3 + i]));
    i++;
  }
  dstr[3] = '\0';
  sprintf_P(day, PSTR("%s %u-%02u-%02u"), dstr, tm_year, tm_mon + 1, dayno + 1);
  sprintf_P(clock, PSTR("%02u:%02u:%02u"), tm_hour, tm_min, tm_sec);
  return (tm_min);
}

void dhcpNtpOptionCallback(uint8_t option, const byte* ptr, uint8_t len)
{
    ntpServerCount = len / 4;

    if (ntpServerCount > MAX_NUM_SERVERS)
        ntpServerCount = MAX_NUM_SERVERS;

    for(int i=0; i<ntpServerCount; i++) {
        ether.printIp("NTP IP: ", ptr);
        EtherCard::copyIp(ntpServers[i], (uint8_t*)ptr);
        ptr += 4;
    }
}

void setup()
{
  Serial.println(F("EtherCard/Nanode NTP Client"));
  Serial.begin(9600);

  currentNtpServer = 0;

  uint8_t rev = ether.begin(sizeof Ethernet::buffer, mymac);
  Serial.print(F("\nENC28J60 Revision "));
  Serial.println(rev, DEC);
  if (rev == 0)
    Serial.println(F("Failed to access Ethernet controller"));

  Serial.println(F("Setting up DHCP"));

  // Register callback for DHCP option 42
  ether.dhcpAddOptionCallback(42, dhcpNtpOptionCallback);

  if (!ether.dhcpSetup())
    Serial.println(F("DHCP failed"));
    
  if (ntpServerCount < 1)
    Serial.println(F("DHCP did not return any NTP servers"));

  ether.printIp("My IP: ", ether.myip);
  ether.printIp("Netmask: ", ether.netmask);
  ether.printIp("GW IP: ", ether.gwip);

  lastUpdate = millis();
}

void loop()
{
  uint16_t dat_p;
  char day[22];
  char clock[22];
  int plen = 0;

  // Main processing loop now we have our addresses
  // handle ping and wait for a tcp packet
  plen = ether.packetReceive();
  dat_p = ether.packetLoop(plen);
  // Has unprocessed packet response
  if (plen > 0) {
    timeLong = 0L;

    if (ether.ntpProcessAnswer(&timeLong, clientPort)) {
      Serial.print(F("Time:"));
      Serial.println(timeLong); // secs since year 1900

      if (timeLong) {
        timeLong -= GETTIMEOFDAY_TO_NTP_OFFSET;
        gmtime(timeLong, day, clock);
        Serial.print(day);
        Serial.print(" ");
        Serial.println(clock);
      }
    }
  }

  // Request an update every 20s
  if (ntpServerCount && lastUpdate + 20000L < millis()) {
    // time to send request
    lastUpdate = millis();
    Serial.print(F("TimeSvr: "));
    Serial.println(currentNtpServer, DEC);

    ether.ntpRequest(ntpServers[currentNtpServer], ++clientPort);
    Serial.print(F("clientPort: "));
    Serial.println(clientPort, DEC);

    if (++currentNtpServer >= ntpServerCount)
      currentNtpServer = 0;
  }

}

// End
