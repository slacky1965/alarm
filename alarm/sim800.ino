bool sim800Check() {
  _response = sim800WriteCmd("AT+CPAS", DEFAULT_TIMEOUT);

  if (!_response) simStatus = false;
  else {
    if (strstr(_response, "+CPAS: 0") || strstr(_response, "+CPAS: 3") || strstr(_response, "+CPAS: 4")) {
      simStatus = true;
      simReset = 0;
    } else simStatus = false;
  }

  if (!simStatus) {
    Serial.println(F("SIM800L not ready! Wait ..."));
    simReset++;
    if (simReset > SIM_RESET) {
      sim800Reset();
    }
  }

  return simStatus;
}

void sim800Init() {

  if (!sim800Check()) return;

  sim800WriteCmd("AT", DEFAULT_TIMEOUT);
  sim800WriteCmd("ATE0", DEFAULT_TIMEOUT);                                  /* Echo mode off        */
  sim800WriteCmd("AT+DDET=1", DEFAULT_TIMEOUT);                             /* Set DTMF code        */
  sim800WriteCmd("AT+CLIP=1", DEFAULT_TIMEOUT);                             /* Set Caller ID        */
  sprintf(tmpBuffer, "AT+CMIC=0,%d", MIC_VOLUME);
  sim800WriteCmd(tmpBuffer, DEFAULT_TIMEOUT);                                      /* Set volume Mic input */
  sim800WriteCmd("AT+CMGF=1;&W", DEFAULT_TIMEOUT);                          /* Set TextMode for SMS */

  /* Delete all SMS after power on of system */
  if (firstStart) {
    sim800DeleteSMS(ALL);
    firstStart = false;
  }
}

char* sim800WriteCmd(const char *cmd, unsigned long waiting) {
  char *resp = NULL;
  Serial.println(cmd);
  SIM800.println(cmd);
  if (waiting > 0) {
    resp = sim800WaitResponse(waiting);
    if (resp) Serial.println(resp);
    else Serial.println(F("Empty response"));
  } else {
    if (_response) free(_response);
    _response = NULL;
  }
  return resp;
}

char* sim800WaitResponse(unsigned long timeout) {
  unsigned int pCount;
  char inChar, *p = NULL;

  unsigned long timeOld = millis();

  while (!SIM800.available() && !(millis() > timeOld + timeout * 1000)) {
    delay(13);
  }

  if (SIM800.available()) {
    for (pCount = 0; SIM800.available(); pCount++) {
      if (pCount == 0 || (pCount % DEFAULT_BUFFER_SIZE) == 0) {
        p = (char*)realloc(_response, DEFAULT_BUFFER_SIZE + pCount);
        if (!p) {
          Serial.println(F("Error realloc in sim800WaitResponse()"));
          free(_response);
          _response = NULL;
          return NULL;
        }
      }
      _response = p;
      inChar = SIM800.read();
      if (pCount == 0 && (inChar == '\r' || inChar == '\n' || inChar == ' ')) {
        pCount--;
        continue;
      }
      p[pCount] = inChar;
      delay(1);
    }
    p[pCount] = 0;
  } else {
    Serial.println(F("Timeout..."));
    simReset++;
  }

  return p;
}

char* sim800ReadDTMF() {
  int in;
  unsigned int i;
  char *cmd = (char*)malloc(DEFAULT_BUFFER_SIZE);
  if (!cmd) {
    Serial.println(F("Error mallac in sim800ReadDTMF()"));
    cmd = NULL;
    return cmd;
  }

  memset((void*)cmd, 0, DEFAULT_BUFFER_SIZE);

  for (i = 0; i < DEFAULT_BUFFER_SIZE && SIM800.available(); i++) {
    in = SIM800.read();
    if (i == 0 && ((char)in == '\r' || (char)in == '\n' || (char)in == ' ')) {
      i--;
      continue;
    }
    cmd[i] = char(in);
    delay(2);
  }
  if (i == 0) {
    free(cmd);
    cmd = NULL;
  }
  return cmd;
}

void sim800Reset() {
  /* Reset SIM800L */
  Serial.println(F("Reset SIM800L ..."));
  simReset = 0;
  digitalWrite(PIN_RESET_SIM, LOW);
  delay(200);
  digitalWrite(PIN_RESET_SIM, HIGH);
}

void sim800DeleteSMS(_deleteSMS DEL) {

  char *at = "AT+CMGDA=\"DEL ";

  switch (DEL) {
    case ALL:
      sprintf(tmpBuffer, "%s%s", at, "ALL\"");
      break;
    case READ:
      sprintf(tmpBuffer, "%s%s", at, "READ\"");
      break;
    case SENT:
      sprintf(tmpBuffer, "%s%s", at, "SENT\"");
      break;
    default:
      tmpBuffer[0] = 0;
      break;
  }

  if (tmpBuffer[0] != 0) {
    sim800WriteCmd(tmpBuffer, DEFAULT_TIMEOUT);
  }
}


void sim800HangUp() {
  sim800WriteCmd("ATH", DEFAULT_TIMEOUT);
}

void sim800PlayTrack(_track track) {
  strcpy_P(tmpBuffer, (char*)pgm_read_word(&(msgName[track])));
  sim800WriteCmd(tmpBuffer, 0);
  delay(DEFAULT_DELAY_PLAY_TRACK * 1000);
}

void sim800StopPlay() {
  sim800WriteCmd("AT+CREC=5", 0);
}

void sim800SendSMS(char* phone, char* message) {
  char at[DEFAULT_BUFFER_SIZE];

  sprintf(at, "AT+CMGS=\"%s\"", phone);
  sim800WriteCmd(at, DEFAULT_TIMEOUT);

  sprintf(at, "\r\n%s\r\n%c", message, (char)26);
  sim800WriteCmd(at, DEFAULT_TIMEOUT);

  deleteSentSMS = true;
}

char* sim800IdxUnreadSMS() {
  char *resp = sim800WriteCmd("AT+CMGL=\"REC UNREAD\",1", DEFAULT_TIMEOUT);
  return resp;
}

char* sim800ReadSMS(int msgIndex) {
  sprintf(tmpBuffer, "AT+CMGR=%u,1", msgIndex);
  /*Serial.print(F("sim800ReadSMS - \"")); Serial.print(tmpBuffer); Serial.println("\"");*/
  char *resp = sim800WriteCmd(tmpBuffer, DEFAULT_TIMEOUT);

  return resp;
}

void sim800SetReadSMS(int msgIndex) {
  sprintf(tmpBuffer, "AT+CMGR=%u", msgIndex);

  sim800WriteCmd(tmpBuffer, DEFAULT_TIMEOUT);
}

void sim800RequestBalance() {
  sprintf(tmpBuffer, "AT+CUSD=1,\"%s\"", BALANCE);
  sim800WriteCmd(tmpBuffer, DEFAULT_TIMEOUT);
}

void sim800Answer() {
  sim800WriteCmd("ATA", DEFAULT_TIMEOUT);
}

void sim800ParseSMS(char* msg) {                                   // Парсим SMS

  /* SMS-commands

      "1*[:1234]"           - alarm on command[:password]
      "0*"                  - alarm off
      "100*[:1234]"         - balance inquiry 
      "2*:2[:+7916xxxxxxx]" - add (or remove) a second phone number to the white list
      "2*:3[:+7916xxxxxxx]" - add (or remove) a third phone number to the white list
      "8*[:1234]"           - to send an SMS with the information about the system

  */

  char *p, *pp, *ppp;
  char *msgHeader = NULL;
  char *msgBody   = NULL;
  char msgPhone[13];
  char newPhone[13];
  char password[5];
  bool phoneWhiteList = false;
  bool parseMsg = false;
  int poz;

  p = strstr(msg, "+CMGR: ");
  if (p) {
    pp = strpbrk(p, "\r");
    if (pp) {
      size_t len = pp - p + 1;
      msgHeader = (char*)malloc(len);
      memset(msgHeader, 0, len);
      if (msgHeader) {
        strncpy(msgHeader, p, pp - p);
        pp = p + strlen(msgHeader) + 2;
        ppp = strstr(p, "OK");
        if (ppp) {
          len = ppp - pp + 1;
          msgBody = (char*)malloc(len);
          memset(msgBody, 0, len);
          if (msgBody) {
            strncpy(msgBody, pp, ppp - pp);
            strtrim(msgBody);
            p = strstr(msgHeader, "\",\"");
            if (p) {
              p += 3;
              pp = strstr(p, "\",\"");
              if (pp) {
                strncpy(msgPhone, p, pp - p);
                msgPhone[12] = 0;
                Serial.print(F("Phone: ")); Serial.println(msgPhone);
                Serial.print(F("Message: ")); Serial.println(msgBody);
                parseMsg = true;
              }
            }
          }
        }
      }
    }
  }

  if (!parseMsg) {
    free(msgHeader);
    free(msgBody);
    Serial.println(F("Parse SMS is false."));
    return;
  }

  if (checkWhiteList(msgPhone)) {
    Serial.println(F("The phone from the whitelist"));
    phoneWhiteList = true;
  } else {
    Serial.println(F("Unknown phonenumber"));
    p = strstr(msgBody, ":");
    if (p) {
      strncpy(password, p + 1, 4);
      password[4] = 0;
      Serial.print(F("password: ")); Serial.println(password);
      if (strcmp(password, Config.guestPassword) != 0) { 
        free(msgHeader);
        free(msgBody);
        return;
      }
    } else {
      /* phone not present in white list and no password */
      free(msgHeader);
      free(msgBody);
      return;
    }
  }

  if (strncmp(msgBody, CMD1, strlen(CMD1)) == 0) {
    Serial.print(F("Check command \"")); Serial.print(CMD1); Serial.println(F("\""));
    if (!alarmOn) alarmOn = true;
  } else if (strncmp(msgBody, CMD0, strlen(CMD0)) == 0) {
    Serial.print(F("Check command \"")); Serial.print(CMD0); Serial.println(F("\""));
    if (alarmOn) alarmOn = false;
  } else if (strncmp(msgBody, CMD2, strlen(CMD2)) == 0) {
    Serial.print(F("Check command \"")); Serial.print(CMD2); Serial.println(F("\""));
    memset(newPhone, 0, sizeof(newPhone));
    p = strstr(msgBody, ":");
    if (p && strlen(msgBody) > 3) {
      poz = (p+1)[0] - '0' ;

      if (poz == 2 || poz == 3) {
        char sms[32];
        p = strstr(p + 1, ":");
        if (p) {
          p++;
          /* Invalid phone format */
          if (!checkPhoneFormat(p)) {
            sprintf(sms, "Invalid phone format \"%s\"", p);
            sim800SendSMS(msgPhone, sms);
            return;
          }
          strcpy(newPhone, p);
        }
        if (strlen(newPhone)) {
          strcpy(Config.phoneWhiteList[poz - 1], newPhone);
          sprintf(sms, "Added phone %d %s", poz, newPhone);
        } else {
          sprintf(sms, "Delete phone %d %s", poz, Config.phoneWhiteList[poz - 1]);
          memset((void*)Config.phoneWhiteList[poz - 1], 0, sizeof(Config.phoneWhiteList[poz - 1]));
        }
        saveConfig();
        sim800SendSMS(msgPhone, sms);
      }
    }
  } else if (strncmp(msgBody, CMD100, strlen(CMD100)) == 0) {
    Serial.print(F("Check command \"")); Serial.print(CMD100); Serial.println(F("\""));
    requestBalance = true;
    strcpy(CallID, msgPhone);
  } else if (strncmp(msgBody, CMD8, strlen(CMD8)) == 0) {
    Serial.print(F("Check command \"")); Serial.print(CMD8); Serial.println(F("\""));
  }

  free(msgHeader);
  free(msgBody);

}


