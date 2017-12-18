
/*
 * This is an example
 */

#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>

#define PIN_RX_SIM    2                         /* to TX pin on SIM800L     */
#define PIN_TX_SIM    7                         /* to RX pin on SIM800L     */
#define PIN_RESET_SIM 9                         /* to RST pin on SIM800L    */
#define PIN_POWER_ON  A0                        /* Pin for conrol ext power */
#define PIN_BATTERY   A1                        /* Pin for control battery  */

#define PASSWORD "1234"                         /* Default user and guest pin in config, when first start */

#define CMD0    "0*"                            /* Alarm Off                */
#define CMD1    "1*"                            /* Alarm On                 */
#define CMD100  "100*"                          /* Request balance          */
#define CMD333  "333*"                          /* Reset SIM800L            */
#define CMD6    "6*"                            /* New guest password       */
#define CMD66   "66*"                           /* New user password        */
#define CMD666  "666*"                          /* Clear config             */
#define CMD9    "9*"                            /* Play info                */
#define CMD2    "2*"                            /* Add new phone in white list (only SMS)  */
#define CMD8    "8*"                            /* Information about the system (only SMS) */

#define BALANCE "#100#"                         /* For MTS                  */

#define SIM_RESET 10                            /* The number of attempts   */

#define MIC_VOLUME 0                            /* Set volume Mic input     */

#define DEFAULT_TIMEOUT 5                       /* Timeout for waiting a response in sec    */
#define DEFAULT_DELAY_PLAY_TRACK 2              /* Default delay after play track in sec   */

/* Voice files in SIM800 memory and command string into PROGMEM */
const char msg0[] PROGMEM = "AT+CREC=4,\"C:\\User\\1.amr\",0,80";
const char msg1[] PROGMEM = "AT+CREC=4,\"C:\\User\\2.amr\",0,80";
const char msg2[] PROGMEM = "AT+CREC=4,\"C:\\User\\3.amr\",0,80";
const char msg3[] PROGMEM = "AT+CREC=4,\"C:\\User\\4.amr\",0,80";
const char msg4[] PROGMEM = "AT+CREC=4,\"C:\\User\\5.amr\",0,80";
const char msg5[] PROGMEM = "AT+CREC=4,\"C:\\User\\6.amr\",0,80";
const char msg6[] PROGMEM = "AT+CREC=4,\"C:\\User\\7.amr\",0,80";
const char msg7[] PROGMEM = "AT+CREC=4,\"C:\\User\\8.amr\",0,80";
const char msg8[] PROGMEM = "AT+CREC=4,\"C:\\User\\9.amr\",0,80";
const char msg9[] PROGMEM = "AT+CREC=4,\"C:\\User\\10.amr\",0,80";
const char msg10[] PROGMEM = "AT+CREC=4,\"C:\\User\\11.amr\",0,80";
const char msg11[] PROGMEM = "AT+CREC=4,\"C:\\User\\12.amr\",0,80";
const char msg12[] PROGMEM = "AT+CREC=4,\"C:\\User\\13.amr\",0,80";

const char* const msgName[] PROGMEM = {msg0, msg1, msg2, msg3, msg4, msg5, msg6, msg7, msg8, msg9, msg10, msg11, msg12};
char fileName[32];
//strcpy_P(fileName, (char*)pgm_read_word(&(fileNameT[_track])));

enum  _track {
  ENTER_PASSWORD = 0,
  NEW_USER_PASSWORD,
  NEW_GUEST_PASSWORD,
  INVALID_PASSWORD,
  WELCOME,
  CMD_EXECUTED,
  NO_CMD,
  ACTIVATION_SENT,
  HELP,
  INFORMATION_SENT,
  ALARM_ON,
  ALARM_OFF,
  CMD_NOT_EXECUTED
};

enum _deleteSMS {
  ALL = 0,
  READ,
  SENT
};


SoftwareSerial SIM800(PIN_RX_SIM, PIN_TX_SIM); /* 2 RX, 7 TX */


typedef struct config {
  char phoneWhiteList[3][13] = {"+7xxxxxxxxxx", "+7xxxxxxxxxx", "+7xxxxxxxxxx"};
  char userPassword[5];
  char guestPassword[5];
} _config;

_config Config;

/* For EEPROM */
#define EEPROM_START 0
bool setEEPROM = false;
uint32_t memcrc; uint8_t *p_memcrc = (uint8_t*)&memcrc;

static const PROGMEM uint32_t crc_table[16] = {
  0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
  0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

String CallID;
String tmpPassword;
String dtmfCmd;
int simReset;
long updatePeriod = 60000;                                        /* Update one time per minute              */
long updateSimStatus = 5000;                                      /* Update SIM800 status one time per 5 sec */
long enterPasswordPeriod = 20000;                                 /* Timeout for enter to password           */
String _response = "";                                            /* Buffer for modem serial answers         */
long lastUpdate = 0, lastCheckSIM = 0, lastCheckSimStatus = 0;

bool firstStart     = false;
bool simStatus      = false;
bool firstCall      = false;
bool guestSession   = false;
bool extPower       = false;
bool requestBalance = false;
bool deleteSentSMS  = false;
bool deleteReadSMS  = false;
bool alarmOn        = false;



void loop() {

  /* Init modem */
  if (!simStatus && lastCheckSimStatus + updateSimStatus < millis()) {
    sim800Init();
    lastCheckSimStatus = millis();
  }

  /* Check status modem one time per minute */
  if (simStatus && lastCheckSIM + updatePeriod < millis()) {
    lastCheckSIM = millis();
    sim800Check();
  }

  if (simStatus) {
    if (lastUpdate + updatePeriod < millis() ) {
      Serial.println();
      Serial.print(F("userPassword: ")); Serial.println(Config.userPassword);
      Serial.print(F("guestPassword: ")); Serial.println(Config.guestPassword);
      while (1) {
        _response = sim800IdxUnreadSMS();                           /* Send request for unread SMS  */
        if (_response.indexOf("+CMGL: ") > -1) {                    /* Get index first unread SMS   */
          int msgIndex = _response.substring(_response.indexOf("+CMGL: ") + 7,
                         _response.indexOf("\"REC UNREAD\"", _response.indexOf("+CMGL: "))).toInt();
          int i;                                               
          for (i = 0; i < 10; i++) {
            _response = sim800ReadSMS(msgIndex);                    /* Read text SMS                */
            _response.trim();                                       
            if (_response.endsWith("OK")) {                         
              if (!deleteReadSMS) deleteReadSMS = true;             /* Up flag for delete read SMS  */
              sim800SetReadSMS(msgIndex);
              sim800ParseSMS(_response);                            /* Parse text SMS               */      
              break;
            } else {
              Serial.println (F("Error answer"));
            }
          }
          /* If 10 times "Error answer", then SMS is bad. Ignored */
          if (i == 10) {
            if (!deleteReadSMS) deleteReadSMS = true;
            sim800SetReadSMS(msgIndex);
          }
          break;
        } else {
          lastUpdate = millis();
          if (deleteReadSMS) {
            /* Delete all read SMS */
            sim800DeleteSMS(READ);
            deleteReadSMS = false;
          }
          break;
        }
      }
    }

    if (SIM800.available())   {                         
      _response = sim800WaitResponse(DEFAULT_TIMEOUT);
      _response.trim();
      Serial.println(_response);

      /* Found USSD answer */
      if (_response.startsWith("+CUSD:")) {
          if (_response.indexOf("\"") > -1) {
            String msgBalance = _response.substring(_response.indexOf("\"") + 1);
            msgBalance = msgBalance.substring(0, msgBalance.indexOf("\""));
            Serial.print(F("USSD: ")); Serial.println(msgBalance);
            /* Send info about balance */
            sim800SendSMS(CallID, msgBalance);
            /* Up flag for delete sent SMS */
            deleteSentSMS = true;
          }
      }      

      /* Detected an incoming call */
      if (_response.startsWith("RING")) {
        CallID = "";
        int phoneindex = _response.indexOf("+CLIP: \"");
        if (phoneindex >= 0) {
          phoneindex += 8;
          /* Get number of incoming call */
          CallID = _response.substring(phoneindex, _response.indexOf("\"", phoneindex));
          Serial.print(F("Number: ")); Serial.println(CallID);
        }

        guestSession = false;

        /* Answer the call */
        sim800Answer();
        sim800PlayTrack(WELCOME);

        /* First call on system (config is empty) */
        if (firstCall) {

          delay(1000);
          sim800StopPlay(0);
          sim800PlayTrack(ENTER_PASSWORD);

          if (enterPassword(enterPasswordPeriod)) {
            strcpy(Config.userPassword, tmpPassword.c_str());
            strcpy(Config.phoneWhiteList[0], CallID.c_str());
            saveConfig();
            firstCall = false;
            Serial.println(F("Activation sent in SMS"));
            sim800StopPlay(0);
            sim800PlayTrack(ACTIVATION_SENT);
            delay(2000);
            sim800HangUp();
            /* Send number of incoming call and entered password to SMS */
            sim800SendSMS(CallID, "Phone: \"" + CallID + "\"\r\n" + "Password: \"" + tmpPassword+ "\"");
            return;
          } else {
            sim800HangUp();
            Serial.println(F("Hang up - timeout"));
            return;
          }

        }

        /* Checking whitelist by number of incoming call */
        if (!checkWhiteList(CallID)) {
          /* If number of incoming call not present in whitelist, then enter guest password */
          if (checkGuestPassword()) {
            guestSession = true;
            sim800StopPlay(0);
            sim800PlayTrack(WELCOME);
          } else {
            sim800HangUp();
            Serial.print(F("Hang up - Invalid guest password: ")); Serial.println(tmpPassword);
          }
        }

        unsigned long timeout = millis(), timeoutDTMF = millis();
        String atCmd;
        dtmfCmd = "";
        delay(1000);
        sim800StopPlay(0);

        /* Reading DTMF of code */
        while (1) {
          atCmd = sim800ReadDTMF();
          delay(100);
          if (atCmd.indexOf("+DTMF: 1") > -1) {
            dtmfCmd += "1";
            timeoutDTMF = millis();
            Serial.print(F("1"));
          } else if (atCmd.indexOf("+DTMF: 2") > -1) {
            dtmfCmd += "2";
            timeoutDTMF = millis();
            Serial.print(F("2"));
          } else if (atCmd.indexOf("+DTMF: 3") > -1) {
            dtmfCmd += "3";
            timeoutDTMF = millis();
            Serial.print(F("3"));
          } else if (atCmd.indexOf("+DTMF: 4") > -1) {
            dtmfCmd += "4";
            timeoutDTMF = millis();
            Serial.print(F("4"));
          } else if (atCmd.indexOf("+DTMF: 5") > -1) {
            dtmfCmd += "5";
            timeoutDTMF = millis();
            Serial.print(F("5"));
          } else if (atCmd.indexOf("+DTMF: 6") > -1) {
            dtmfCmd += "6";
            timeoutDTMF = millis();
            Serial.print(F("6"));
          } else if (atCmd.indexOf("+DTMF: 7") > -1) {
            dtmfCmd += "7";
            timeoutDTMF = millis();
            Serial.print(F("7"));
          } else if (atCmd.indexOf("+DTMF: 8") > -1) {
            dtmfCmd += "8";
            timeoutDTMF = millis();
            Serial.print(F("8"));
          } else if (atCmd.indexOf("+DTMF: 9") > -1) {
            dtmfCmd += "9";
            timeoutDTMF = millis();
            Serial.print(F("9"));
          } else if (atCmd.indexOf("+DTMF: 0") > -1) {
            dtmfCmd += "0";
            timeoutDTMF = millis();
            Serial.print(F("0"));
          } else if (atCmd.indexOf("+DTMF: *") > -1) {
            dtmfCmd += "*";
            Serial.println(F("*"));
            timeout = millis();
            if (dtmfCmd == CMD1) {
              Serial.print(F("Check command \"")); Serial.print(CMD1); Serial.println(F("\""));
              if (!alarmOn) {
                alarmOn = true;
                sim800StopPlay(0);
                sim800PlayTrack(CMD_EXECUTED);
                sim800StopPlay(0);
                sim800PlayTrack(ALARM_ON);
              } else {
                sim800StopPlay(0);
                sim800PlayTrack(CMD_NOT_EXECUTED);
              }
            } else if (dtmfCmd == CMD0) {
              Serial.print(F("Check command \"")); Serial.print(CMD0); Serial.println(F("\""));
              if (alarmOn) {
                alarmOn = false;
                sim800StopPlay(0);
                sim800PlayTrack(CMD_EXECUTED);
                sim800StopPlay(0);
                sim800PlayTrack(ALARM_OFF);
              } else {
                sim800StopPlay(0);
                sim800PlayTrack(CMD_NOT_EXECUTED);
              }
            }  else if (dtmfCmd == CMD100) {
              Serial.print(F("Check command \"")); Serial.print(CMD100); Serial.println(F("\""));
              sim800StopPlay(0);
              sim800PlayTrack(INFORMATION_SENT);
              delay(2000);
              sim800HangUp();
              requestBalance = true;
              timeout = 0;
            } else if (dtmfCmd == CMD333) {
              Serial.print(F("Check command \"")); Serial.print(CMD333); Serial.println(F("\""));
              Serial.println(F("Enter password"));
              sim800StopPlay(0);
              sim800PlayTrack(ENTER_PASSWORD);
              if (checkUserPassword()) {
                sim800StopPlay(0);
                sim800PlayTrack(CMD_EXECUTED);
                delay(2000);
                sim800HangUp();
                sim800Reset();
                timeout = 0;
              } else {
                Serial.println(F("Invalid user password!!!"));
                sim800StopPlay(0);
                sim800PlayTrack(INVALID_PASSWORD);
              }
            } else if (dtmfCmd == CMD9) {
              Serial.print(F("Check command \"")); Serial.print(CMD9); Serial.println(F("\""));
              sim800StopPlay(0);
              sim800PlayTrack(HELP);
            } else if (dtmfCmd == CMD6) {
              Serial.print(F("Check command \"")); Serial.print(CMD6); Serial.println(F("\""));
              Serial.println("Введите пароль");
              sim800StopPlay(0);
              sim800PlayTrack(ENTER_PASSWORD);
              if (checkUserPassword()) {
                Serial.println(F("Enter new guest password"));
                sim800StopPlay(0);
                sim800PlayTrack(NEW_GUEST_PASSWORD);
                if (enterPassword(enterPasswordPeriod)) {
                  strcpy(Config.guestPassword, tmpPassword.c_str());
                  saveConfig();
                  sim800StopPlay(0);
                  sim800PlayTrack(CMD_EXECUTED);
                  sim800SendSMS(CallID, "New guest password: "+(String)Config.guestPassword);
                }
              } else {
                Serial.println(F("Invalid user password!!!"));
                sim800StopPlay(0);
                sim800PlayTrack(INVALID_PASSWORD);
              }
            } else if (dtmfCmd == CMD66) {
              Serial.print(F("Check command \"")); Serial.print(CMD66); Serial.println(F("\""));
              Serial.println(F("Enter password"));
              sim800StopPlay(0);
              sim800PlayTrack(ENTER_PASSWORD);
              if (checkUserPassword()) {
                Serial.println(F("Enter new user password"));
                sim800StopPlay(0);
                sim800PlayTrack(NEW_USER_PASSWORD);
                if (enterPassword(enterPasswordPeriod)) {
                  strcpy(Config.userPassword, tmpPassword.c_str());
                  saveConfig();
                  sim800StopPlay(0);
                  sim800PlayTrack(CMD_EXECUTED);
                  sim800SendSMS(CallID, "New user password: "+(String)Config.userPassword);
                }
              } else {
                Serial.println(F("Invalid user password!!!"));
                sim800StopPlay(0);
                sim800PlayTrack(INVALID_PASSWORD);
              }
            } else if (dtmfCmd == CMD666) {
              Serial.print(F("Check command \"")); Serial.print(CMD666); Serial.println(F("\""));
              sim800StopPlay(0);
              sim800PlayTrack(ENTER_PASSWORD);
              if (checkUserPassword()) {
                clearEeprom();
                initDefaultConfig(&Config);
                firstCall = true;
                timeout = 0;
                Serial.println(F("Clear config!!!"));
                sim800StopPlay(0);
                sim800PlayTrack(CMD_EXECUTED);
                delay(2000);
              } else {
                Serial.println(F("Invalid user password!!!"));
                sim800StopPlay(0);
                sim800PlayTrack(INVALID_PASSWORD);
              }
            } else {
              Serial.println(F("No command"));
              sim800StopPlay(0);
              sim800PlayTrack(NO_CMD);
            }
            dtmfCmd = "";
            timeoutDTMF = millis();
            Serial.println();
          } else if (atCmd.indexOf("+DTMF: #") > -1) {
            sim800StopPlay(0);
            sim800PlayTrack(WELCOME);
            Serial.println("#");
            Serial.println(F("\nBegin now"));
            dtmfCmd = "";
            timeout = millis();
            timeoutDTMF = millis();
          } else if (atCmd.indexOf("NO CARRIER") > -1 || (timeout + updatePeriod) < millis()) {
            sim800HangUp();
            break;
          } else {
            /* If the timeout between the enter DTMF code more than 10 seconds, to reset the input */
            if (dtmfCmd.length() > 0 && dtmfCmd.indexOf("*") == -1 && (millis() - timeoutDTMF) > 10000) {
              dtmfCmd = atCmd = "";
              timeoutDTMF = millis();
              Serial.println(F("Reset DTMF code"));
              continue;
            }
          }
        }

      }

      /* New message has been received */
      if (_response.indexOf("+CMTI:") > -1) {
        lastUpdate = 0;
      }

    }

    /* Reuest balance */
    if (requestBalance) {
      sim800RequestBalance();
      requestBalance = false;
      lastUpdate = millis();
    }

    /* Delete all sent SMS */
    if (deleteSentSMS) {
      sim800DeleteSMS(SENT);
      deleteSentSMS = false;
    }
      
    if (digitalRead(PIN_POWER_ON)) {
      if (!extPower) {
        extPower = true;
        Serial.println(F("External power is High"));
      }
    } else {
      if (extPower) {
        extPower = false;
        Serial.println(F("External power is Low"));
      }
    }


    if (Serial.available())  {

        SIM800.write(Serial.read());
    }


  }

}

