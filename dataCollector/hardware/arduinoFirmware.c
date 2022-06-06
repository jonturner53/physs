// Important note about Arduino C compiler.
// It does not correctly compile programs with variable declarations 
// in case clauses. The compiler does not produce an error message,
// but generates bad code. Advice: place all declarations at start
// of function.

#include <Wire.h>

// comment out this line for arduino case
//#define NUCLEO 1

#ifdef NUCLEO

const int refCtl = PA12;  // set low in setup and never changed
const int pvPwr = PB0;
const int litePwr = PB1;

const int pump[] = { 0, PB7, PB6 };
const int valve[] = { 0, PA11, PB5 };

const int deut = PA8;
const int tung = PC15;
const int shut = PC14;

const int procOff = PA0;   // assert to turn off main processor
const int commOff = PB4;   // assert to turn on comm device

const int leakCtl = PB3;
const int leak = PA7;

const int temp = PA4;
const int pres1 = PA1;
const int pres2 = PA3;
const int vbat = PA2;

#else

const int refCtl = 2;  // set low in setup and never changed
const int pvPwr = 3;
const int litePwr = 6;

const int pump[] = { 0, 4, 5, 80, 81, 82, 83 };
const int valve[] = { 0, 10, 11, 84, 85, 86, 87 };

const int deut = 9;
const int tung = 8;
const int shut = 7;

const int procOff = A0;   // assert to turn off main processor
const int commOff = 12;   // assert to turn on comm device

const int leakCtl = 13;
const int leak = A6;

const int temp = A3;
const int pres1 = A1;
const int pres2 = A2;
const int vbat = A7;

#endif

// i2c addresses of devices on bus
const int dac1 = 16;
const int dac2 = 18;
const int portXtender = 34;

int failureDetected = 0;  // set if critical failure detected
int shutdownComplete = 0;   // set after a critical shutdown completes
int procPowerState = 0;   // set when main processor power is on
int check4faults = 0;     // when set check for failures, otherwise ignore

int standAlone = 1;     // set when arduino running by itself
int noisy = 0;      // used as flag to enable debugging code

void initLog();
void log(char*);
void printLog();
void configurePortXtender();
void configureDACs();
void setPump(int, int);
bool setTime(char*);
bool getTime(char*);

void setup() { 
  shutdownComplete = 0; failureDetected = 0;
  check4faults = 0; standAlone = 1;

  initLog(); log("setup");

  pinMode(refCtl, OUTPUT); digitalWrite(refCtl, LOW);
  pinMode(pvPwr, OUTPUT); digitalWrite(pvPwr, LOW);
  pinMode(litePwr, OUTPUT); digitalWrite(litePwr, LOW);
  pinMode(pump[1], OUTPUT); digitalWrite(pump[1], LOW);
  pinMode(pump[2], OUTPUT); digitalWrite(pump[2], LOW);
  pinMode(valve[1], OUTPUT); digitalWrite(valve[1], LOW);
  pinMode(valve[2], OUTPUT); digitalWrite(valve[2], LOW);
  pinMode(deut, OUTPUT); digitalWrite(deut, LOW);
  pinMode(tung, OUTPUT); digitalWrite(tung, LOW);
  pinMode(shut, OUTPUT); digitalWrite(shut, LOW);
  pinMode(procOff, OUTPUT); digitalWrite(procOff, LOW);
  pinMode(commOff, OUTPUT); digitalWrite(commOff, LOW);
  pinMode(leakCtl, OUTPUT); digitalWrite(leakCtl, LOW);

  procPowerState = 1;

  Wire.begin();
  configurePortXtender(); configureDACs();
  setPump(1, 2048); setPump(2, 2048);
  setPump(3, 0); setPump(4, 0); setPump(5, 0); setPump(6, 0);

  power(0, 0); // turn off power to pumps, lights, etc

  Serial.begin(115200);
  Serial.setTimeout(2);
  while (!Serial) {}
}

/** Configure the port extender on power up.  */
void configurePortXtender() {
  unsigned char buf[2];  // buffer for i2c commands

  // set iodir registers to all zeros (configuring pins as outputs)
  buf[0] = 0x00; buf[1] = 0x00;
  i2cSend(portXtender, buf, 2);
  buf[0] = 0x01; 
  i2cSend(portXtender, buf, 2);

  // initialize most pins to 0
  buf[0] = 0x12;
  i2cSend(portXtender, buf, 2);
  buf[0] = 0x13; buf[1] = 0x02;
  i2cSend(portXtender, buf, 2);
}

/** Write to a port extender pin
 *  @param pin is a pin number (0-15)
 *  @param state is desired pin state (0 or 1)
 */
void writeXtender(int pin, int state) {
  static unsigned int xtenderPins = 0;

  if (state == 0) xtenderPins &= (~(1 << pin));
  else xtenderPins |= (1 << pin);

  unsigned char buf[2];
  buf[0] = (pin < 8 ? 0x12 : 0x13);
  buf[1] = (pin < 8 ? xtenderPins & 0xff : (xtenderPins >> 8) & 0xff);
  i2cSend(portXtender, buf, 2);
}

/** Configure digital-to-analog converters.
 *  There are two 4-channel DAC chips.
 *  The first is used to control pumps 1 and 2 which are bidirectional.
 *  The second is used to control pumps 4-6 which are unidirectional.
 *  Both DACs are configured to use the external reference voltage
 *  The first two channels of the first DAC are initialized to 2048
 *  and the last two channels are powered off. All four channels
 *  of the second DAC are initialized to 0.
 *  As a side-effect, clear standAlone flag if a positive ACK is
 *  received from the first DAC.
 */
void configureDACs() {
  unsigned char buf[3]; int i;

  // select external reference voltage
  buf[0] = 0x70; buf[1] = buf[2] = 0;

  int status = i2cSend(dac1, buf, 3);

  if (status == 0) standAlone = 0;
  i2cSend(dac2, buf, 3);

  // power off channels 2 and 3 of first DAC
  buf[0] = 0x42; i2cSend(dac1, buf, 3);
  buf[0] = 0x43; i2cSend(dac1, buf, 3);

  // set first two channels of first DAC to mid-range,
  // all channels of second DAC to zero
  for (i = 0; i <= 1; i++) writeDAC(dac1, i, 2048);
  for (i = 0; i <= 3; i++) writeDAC(dac2, i, 0);
}

/** Write a value to a channel on a specified DAC.
 *  @param dac is the address of the desired DAC.
 *  @param chan is the selected DAC channel (0-3)
 *  @param value is the requested value (12 bits)
 */
void writeDAC(int dac, int chan, int value) {
  unsigned char buf[3]; 
  buf[0] = 0x30 | (chan & 3);
  buf[1] = (value >> 4) & 0xff;
  buf[2] = (value & 0xf) << 4;
  i2cSend(dac, buf, 3);
}

/** Send information on i2c bus.
 *  @param adr is the i2c address of the target device on the bus
 *  @param buf is a byte buffer
 *  @param n is the number of bytes in buf
 */
int i2cSend(int adr, unsigned char buf[], int n) {
  Wire.beginTransmission(adr);
  int i;
  for (i = 0; i < n; i++) Wire.write(buf[i]);
  return Wire.endTransmission();
}

/** Set a valve to a specified state.
 *  @param v is a valve number (1-6)
 *  @param state is the desired state (0 or 1)
 */
void setValve(int v, int state) {
  writePin(valve[v], state);
}

/** Set pump to a specified speed and turn pump power on/off as appropriate.
 *  @param p is a pump number (1-6)
 *  @param speed is a pump speed (0-4095)
 */
void setPump(int p, int speed) {
  int dac = (p <= 2 ? dac1 : dac2);
  int chan = (p <= 2 ? p-1 : p-3);
  writeDAC(dac, chan, speed);
  if (p <= 2) writePin(pump[p], (speed == 2048 ? 0 : 1));
  else writePin(pump[p], (speed == 0 ? 0 : 1));
}

/** Write to a digital pin on either the Arduino or the port extender.
 *  @param pin is a pin number; values <80 are interpreted as Arduino
 *  pins, values larger than 80 are interpreted as pins on the extender.
 *  @param state is the desired pin state (0 or 1)
 */
void writePin(int pin, int state) {
  if (pin < 80) digitalWrite(pin, (state == 0 ? LOW : HIGH));
  else writeXtender(pin-80, state);
}

void lights(int d, int t, int s) {
  writePin(deut, d); writePin(tung, t); writePin(shut, s);
}

void power(int pv, int lite) {
  writePin(pvPwr, pv); writePin(litePwr, lite);
}

int check4leak() {
  static int count = 0;
  static unsigned long lasttime = 0;
  unsigned long now = millis();
  if (now - lasttime <  30 && count++ < 50) {
    log("leak");
    lasttime = now;
  }
  digitalWrite(leakCtl, HIGH);
  delay(5);
  int result = analogRead(leak);
  digitalWrite(leakCtl, LOW);
  return (result < 500);
}

/** Convert a character string to an integer.
 *  @param p is a pointer to a numeric string
 *  @param n is number of characters in numeric string
 *  @return the resulting integer
 */
int ctoi(char *p, int n) {
  int i = 0;
  while (n > 0) {
    i = (i * 10) + (*p - '0');
    p++; n--;
  }
  return i;
}

/** Convert a hex string to a binary buffer.
 *  @param hex is a pointer to a buffer of characters
 *  containing hex digits and spaces
 *  @param n is the number of characters in the hex string
 *  @param buf is a pointer to a buffer in which results
 *  are returned; every two hex digits in hex produces a single
 *  byte in buf; anything in hex that is not a hex digit is ignored
 *  @return the number of bytes in buf.
 */
int hex2bytes(char *hex, int n, char *buf) {
  int i = 0; int digit;
  while (n > 0) {
    digit = -1;
    if (*hex >= '0' && *hex <= '9')
      digit = *hex - '0';
    else if (*hex >= 'a' && *hex <= 'f')
      digit = 10 + *hex - 'a';
    if (digit >= 0) {
      if (i&1 == 0) {
        buf[i/2] = digit;
      } else {
        buf[i/2] = buf[i/2] * 16 + digit;
      }
      i++;
    }
    hex++; n--;
  }
  return (i/2) + (i&1);
}

/** Convert a non-negative integer to a string.
 *  @param buf is a character buffer in which the result is returned;
 *  it is assumed to be long enough
 *  @param n is a non-negative integer.
 *  @return pointer to end-of-string character in buf
 */
char* int2string(char *buf, int n) {
  int i = n; int j = 0;
  if (i == 0) {
    buf[0] = '0'; buf[1] = '\0';
    return &buf[1];
  }
  while (i > 0) {
    i /= 10; j++;
  }
  char *p = &buf[j];
  buf[j--] = '\0';
  while (j >= 0) {
    buf[j--] = (n%10) + '0'; n /= 10;
  }
  return p;
}

/** Convert an unsigned long to a string.
 *  @param buf is a character buffer in which the result is returned;
 *  it is assumed to be long enough
 *  @param n is an integer.
 *  @return pointer to end-of-string character in buf
 */
char* ulong2string(char *buf, unsigned long n) {
  unsigned long i = n; int j = 0;
  if (i == 0) {
    buf[0] = '0'; buf[1] = '\0';
    return &buf[1];
  }
  while (i > 0) {
    i /= 10; j++;
  }
  char *p = &buf[j];
  buf[j--] = '\0';
  while (j >= 0) {
    buf[j--] = (n%10) + '0'; n /= 10;
  }
  return p;
}

/** Copy a string and return pointer to end of string */
char *stringCopy(char *to, char *from, int n) {
  while (--n && *from != '\0') {
    *to++ = *from++;
  }
  *to = '\0';
  return to;
}

/** Command interpreter loop.
 *  Commands are sent one to a line and are identified by a single
 *  letter, followed by command-specific arguments.
 *
 *  es..s  echo back the string s..s
 *  vns    switch valve n (1-6) to state s
 *  pnssss set pump n (1-6) speed to ssss; turn on when ssss!=0
 *  s      return status report
 *  ldts   set light state (deuterium, tungsten, shutter)
 *  Ppl    power controls pumps and valves, light source
 *  Sm..m  put main processor to sleep for m..m minutes
 *  Ms     turn power to comm device to state s (0 or 1)
 *  N      go to noisy mode (allow debugging messages)
 *  Q      go to quiet mode (no debugging messages)
 *  Dppb   set digital output pp to state b
 *  Ap     return value of analog pin p
 *  Iaa hh hh .. send a sequence of bytes to i2c bus;
 *         each pair of hex digits encodes a single byte;
 *         the first pair of hex digits (aa) is the 7 bit address
 *  Fx     check for faults if x=1, else do not check
 *  H      return 0 if arduino operating standAlone, else 1
 *  x      return contents of log message buffer
 */
void loop() { 
  // processor power state variables
  static unsigned long procSleepInterval = 0;
  static unsigned long procSleepTime;
  static int waiting2sleep = 0;
  static unsigned long failureTime = 0;

  // average analog data values
  static int battery = 0;
  static int temperature = 0;
  static int pressure1 = 0;
  static int pressure2 = 0;
  
  // sums of analog readings since last update
  static long bsum = 0;
  static long tsum = 0;
  static long p1sum = 0;
  static long p2sum = 0;

  // used to control reading of of analog input signals
  static unsigned long lastUpdate = 0;
  static unsigned long now = 0;
  static long count = 1;

  // temporary buffer and associated pointer
  char msgBuf[100]; char *p;

  // if (shutdownComplete) return;

  // wake up main processor if it's time to do so
  // initial delay of 30 seconds
  if (procSleepInterval > 0) {
    unsigned long t = millis();
    if (waiting2sleep && t - procSleepTime > 30000ul) {
      log("proc off");
      writePin(procOff, 1); writePin(commOff, 1); power(0,0);
      procPowerState = 0; waiting2sleep = 0;
    } else if (t - procSleepTime > procSleepInterval) {
      log("proc on");
      writePin(commOff, 0); writePin(procOff, 0);
      procPowerState = 1; procSleepInterval = 0;
    }
  }

  // accumulate analog input values
  bsum += analogRead(vbat);
  tsum += analogRead(temp);
  p1sum += analogRead(pres1);
  p2sum += analogRead(pres2);

  // update average values for analog inputs
  now = micros();
  if (now - lastUpdate < 16000) {
    count++;
  } else {
    battery = bsum/count;
    temperature = tsum/count;
    pressure1 = p1sum/count;
    pressure2 = p2sum/count;
    bsum = tsum = p1sum = p2sum = 0;   
    lastUpdate = now; count = 1;

    // and check for critical failures
    if (check4faults && !shutdownComplete) {
      if (failureDetected) {
        if ((now - failureTime) > 500000) {
          log("shutdown");
          power(0,0); writePin(procOff, 1); writePin(commOff, 1);
          procPowerState = 0; shutdownComplete = 1; return;
        }
      } else if (!standAlone && (battery < 600 || temperature > 768 ||
                 check4leak())) {
        log("fault detected");
        if (procPowerState) {
          failureDetected = 1; failureTime = now;
        } else {
          shutdownComplete = 1; return;
        }
      }
    }
  }
  
  int i, n, m, pin, adr, pspeed, state;
  char cmd, buf[100], bytes[100];

  static int outOfSync = 0;

  n = Serial.readBytesUntil('\n', buf, sizeof(buf)-2);
    // because readBytes may return without matching \n,
    // use second delimiter . to verify that we really do have complete command;
    // if we get out of sync, discard until we get back in
  if (outOfSync) {
    if (n > 0 && buf[n-1] == '.') outOfSync = 0; 
    n = 0;
  } else if (n > 0 && buf[n-1] == '.') {
    buf[--n] = '\0';  // complete line
  } else if (n > 0) { 
    buf[0] = '\0'; n = 0; // ignore truncated command
    outOfSync = 1; 
  }

  if (n > 0) {
    buf[n] = '\0';
    cmd = buf[0];
    if (cmd != 'R' && cmd != 'C' && cmd != 's' && cmd != 'x') {
      log(buf);
    }
    switch (cmd) {
    case 'e':
      // es..s echo back the string s..s
      Serial.print(&buf[1]); Serial.print(".\n");
      break;
    case 'v':
      // vns switch valve n (1-6) to state s
      i = buf[1]-'0'; state = buf[2]-'0';
      setValve(i, state);
      if (noisy) {
        Serial.print("Setting valve "); Serial.print(i);
        Serial.print(" to state "); Serial.print(state);
      }
      Serial.print(".\n");
      break;
    case 'p':
      // pnssss set pump n (1-6) speed to ssss; turn on when ssss!=0
      i = buf[1] - '0';
      pspeed = ctoi(&buf[2], n-2);
      setPump(i, pspeed);
      if (noisy) {
        Serial.print("Setting pump "); Serial.print(i);
        Serial.print(" to speed "); Serial.print(pspeed);
      }
      Serial.print(".\n");
      break;
    case 's':
      // generate status report
      p = int2string(msgBuf, battery); *p++ = ' '; *p = '\0';
      p = int2string(p, temperature); *p++ = ' '; *p = '\0';
      p = int2string(p, pressure1); *p++ = ' '; *p = '\0';
      p = int2string(p, pressure2); *p++ = ' '; *p = '\0';
      p = int2string(p, check4leak()); *p++ = ' '; *p = '\0';
      if (!getTime(p)) {
        *p++ = 'E'; *p = '\0';
      }
      Serial.print(msgBuf); Serial.print(".\n");
      break;
    case 'l':
      // ldts   set light state (deuterium, tungsten, shutter)
      lights(buf[1]-'0', buf[2]-'0', buf[3]-'0');
      Serial.print(".\n");
      break;
    case 'P':
      // Ppl   power controls pumps and valves, light source
      if (noisy) {
        Serial.print("Setting power state to ");
        Serial.print(&buf[1]);
      }
      Serial.print(".\n");
      power(buf[1]-'0', buf[2]-'0');
      break;
    case 't':
      // t  read real-time clock; return string ss mm hh dd DD MM YY
      if (getTime(buf)) {
        Serial.print(buf); 
      } else {
        Serial.print("E");
      }
      Serial.print(".\n");
      break;
    case 'T':
      // T ss mm hh dd DD MM YY  set real-time clock
      setTime(&buf[2]);
      Serial.print(".\n");
      break;
    case 'S':
      // Sm..m put main processor to sleep for m..m minutes
      // after a one minute delay
      i = ctoi(&buf[1], n-1);
      if (i == 0) {
        // special case used when shutting down
        // means that main processor has completed essential shutdown steps
        if (failureDetected) {
          power(0,0); writePin(procOff, 1); writePin(commOff, 1);
          procPowerState = 0; shutdownComplete = 1;
        }
      } else if (procSleepInterval == 0) {
        procSleepInterval = (60000ul * i) + 30000ul;
               // add 30 seconds for initial delay
        procSleepTime = millis(); waiting2sleep = 1;
      }
      if (noisy) {
        Serial.print("Putting processor to sleep for "); Serial.print(i);
        Serial.print(" minutes after 30 second delay");
      }
      Serial.print(".\n");
      break;
    case 'M':
      state = buf[1]-'0';
      writePin(commOff, (state ? 0 : 1));
      if (noisy) {
        Serial.print("Turning comm link ");
        Serial.print(state == 0 ? "off.\n" : "on");
      }
      Serial.print(".\n");
      break;
    case 'N': 
      Serial.print("going to noisy mode.\n");
      noisy = 1; break;
    case 'Q':
      Serial.print("going to quiet mode.\n");
      noisy = 0; break;
    case 'D':
      // Dppb   set digital output pp to state b
      pin = ctoi(&buf[1], n-2); state = buf[n-1]-'0';
      writePin(pin, state);
      if (noisy) {
        Serial.print("Setting digital output "); Serial.print(pin);
        Serial.print(" to state "); Serial.print(state);
      }
      Serial.print(".\n");
      break;
    case 'A':
      // Ap return value of analog pin p
      pin = buf[1]-'0';
      Serial.print(analogRead(pin)); Serial.print(".\n");
      break;
    case 'I':
      // Iaa hh hh .. send a sequence of bytes to i2c bus;
      // each pair of hex digits encodes a single byte;
      // the first pair of hex digits (aa) is the 7 bit address
      m = hex2bytes(&buf[1], n-1, bytes);
      adr = bytes[0];
      i2cSend(adr, (unsigned char*) &bytes[1], m-1);
      if (noisy) {
        Serial.print("sending "); Serial.print(m);       
        Serial.print(" bytes to i2c device with address ");
        buf[0] = (adr >> 4) & 0xf; buf[1] = adr & 0xf; buf[2] = '\0';
        buf[0] = (buf[0] <= 9 ? '0' + buf[0] : 'a' + buf[0] - 10);
        buf[1] = (buf[1] <= 9 ? '0' + buf[1] : 'a' + buf[1] - 10);
        Serial.print(buf);
      }
      Serial.print(".\n");
      break;
    case 'F':
      // Fx enable fault detection if x=1, else disable
       check4faults = (buf[1] == '1' ? 1 : 0);
      if (noisy) {
        Serial.print("Turning fault monitoring ");
        Serial.print(buf[1] == '1' ? "on.\n" : "off");
      }
      Serial.print(".\n");
      break;
    case 'H':
      // check for presence of hardware
      Serial.print(standAlone == 1 ? 0 : 1); Serial.print(".\n");
      break;
    case 'x':
      // get log messages
      printLog();
      break;
    default:
      break;
    }
  }
}

char logBuf[512]; // buffer used to store log messages
char *logNext;    // pointer to free space in logBuf

void initLog() { logNext = logBuf;  *logNext = '\0'; }

/* Print the contents of the log buffer to the serial link. */
void printLog() {
  Serial.print(logBuf); Serial.print(".\n");
  logNext = logBuf; *logNext = '\0';
}

/* Add a message to the log buffer. */
void log(char *msg) {
  char *p = msg; int n = 0;
  while (*p++ != '\0') n++;
  if (logNext + n + 20 >= logBuf + sizeof(logBuf))
    return; // ignore excess
  p = msg; 
  while (*p != '\0') *logNext++ = *p++;
  // add timestamp
  *logNext++ = ' '; *logNext++ = '[';
  unsigned long t = millis() / 1000ul;
  logNext = ulong2string(logNext, t);
  *logNext++ = ']'; *logNext++ = ' '; *logNext = '\0';
}

const uint8_t clkAdr = 0x68;

/** Set the real-time clock.
 *  @param tbuf is a character buffer containing seven 2-digit ascii
 *  numeric values, separated by spaces
 */
bool setTime(char *tbuf) {
  Wire.beginTransmission(clkAdr);
  int i;
  Wire.write(0);
  for (i = 0; i < 7; i++) {
    Wire.write(((tbuf[0] - '0') << 4) + (tbuf[1] - '0')); tbuf += 3;
  }
  return Wire.endTransmission() ? false : true;
}

/** Read time value from RTC and copy it to a string.
 *  @param tbuf is a character buffer in which result is returned;
 *  it is assumed to have space for at least 23 characters
 *  @return true if able to read time value, else false
 */
bool getTime(char *tbuf) {
  /* set control register
  Wire.beginTransmission(clkAdr);
  Wire.write(0x0e); Wire.write(0x04);
  if (Wire.endTransmission()) return false;
  */

  // read date/time registers
  Wire.beginTransmission(clkAdr);
  Wire.write(0);
  if (Wire.endTransmission()) return false;
  
  Wire.requestFrom(clkAdr, (uint8_t) 7);
  int v; int i = 0;
  while (i < 7 && Wire.available()) {
    v = Wire.read();
    *tbuf++ = ((v >> 4) & 0xf) + '0'; *tbuf++ = (v & 0xf) + '0'; *tbuf++ = ' ';
    i++;
  }
  *tbuf = '\0';
  if (i != 7) return false;
  return true;
}
