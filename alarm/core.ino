void clearEeprom() {
  //  EEPROM.begin(sizeof(_config)+sizeof(memcrc));

  for (int i = EEPROM_START; i < EEPROM_START + sizeof(_config) + sizeof(memcrc); i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.end();
}

unsigned long crc_update(unsigned long crc, byte data) {
  byte tbl_idx;
  tbl_idx = crc ^ (data >> (0 * 4));
  crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
  tbl_idx = crc ^ (data >> (1 * 4));
  crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
  return crc;
}

unsigned long crc_byte(byte *b, int len) {
  unsigned long crc = ~0L;
  int i;

  for (i = 0 ; i < len ; i++) {
    crc = crc_update(crc, *b++);
  }
  crc = ~crc;
  return crc;
}



bool checkWhiteList(char* msgPhone) {

  char phone[13];
  int i;

  for (i = 0; i < 3; i++) {
    strcpy(phone, Config.phoneWhiteList[i]);;
    Serial.print(i + 1); Serial.print(F(". ConfigPhone: ")); Serial.println(phone);
    if (strlen(msgPhone) > 6 && strcmp(phone, msgPhone) == 0) return true;
  }
  return false;
}

bool checkGuestPassword() {
  enterPassword(enterPasswordPeriod);
  if (strcmp(tmpBuffer, Config.guestPassword) != 0 ) return false;
  return true;
}

bool checkUserPassword() {
  enterPassword(enterPasswordPeriod);
  if (strcmp(tmpBuffer, Config.userPassword) != 0 ) return false;
  return true;
}

bool enterPassword(unsigned long period) {
  unsigned long timeout = millis();
  char *p, *atCmd = NULL;
  int i = 0;
  bool pinEnt = false;
  memset((void*)tmpBuffer, 0, sizeof(tmpBuffer));

  do {
    free(atCmd);
    atCmd = sim800ReadOnLine();
    if (!atCmd) continue;
    delay(100);
    p = strstr(atCmd, DTMF);
    if (p) {
      if (*(p+7) == '#') {
        memset(tmpBuffer, 0, sizeof(tmpBuffer));
        Serial.print(F("#"));
        break;
      }
      tmpBuffer[i++] = *(p+7);
      Serial.print(*(p+7));
    }
    p = strstr(atCmd, NO_CARRIER);
    if (p) {
      break;
    }
    if (strlen(tmpBuffer) == 4) {
      pinEnt = true;
    }

  } while (!pinEnt && i < 5 && (timeout + period) > millis());

  free(atCmd);

  Serial.println();

  return pinEnt;

}


void strtrim(char *s) {

  /* With begin */
  int i = 0, j;
  while ((s[i] == ' ') || (s[i] == '\t')) {
    i++;
  }
  if (i > 0) {
    for (j = 0; j < strlen(s); j++)
    {
      s[j] = s[j + i];
    }
    s[j] = '\0';
  }

  /* With end */
  i = strlen(s) - 1;
  while ((s[i] == ' ') || (s[i] == '\t') || (s[i] == '\r') || (s[i] == '\n')) {
    i--;
  }
  if (i < (strlen(s) - 1)) {
    s[i + 1] = '\0';
  }
}

bool isNumber(char c) {
  return (c >= '0' && c <= '9');
}

bool checkPhoneFormat(char* phone) {
  int i, len = strlen(phone);

  if (phone[0] != '+' || len != sizeof(Config.phoneWhiteList[0])-1) return false;

  for (i = 1; i < len; i++) {
    if (!isNumber(phone[i])) return false;
  }
  return true;  
}

