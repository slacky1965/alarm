void clearEeprom() {
//  EEPROM.begin(sizeof(_config)+sizeof(memcrc));

  for (int i = EEPROM_START; i < EEPROM_START+sizeof(_config)+sizeof(memcrc); i++) {
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



bool checkWhiteList(String msgPhone) {

  String phone;
  int i;

  for (i = 0; i < 3; i++) {
    phone = Config.phoneWhiteList[i];
    Serial.print(i+1); Serial.print(F(". ConfigPhone: ")); Serial.println(phone);
    if (msgPhone.length() > 6 && phone.indexOf(msgPhone) > -1) return true;
  }
  return false;  
}

bool checkGuestPassword() {
  enterPassword(enterPasswordPeriod);
  if (strcmp(tmpPassword.c_str(), Config.guestPassword) != 0 ) return false;
  return true;
}

bool checkUserPassword() {
  enterPassword(enterPasswordPeriod);
  if (strcmp(tmpPassword.c_str(), Config.userPassword) != 0 ) return false;
  return true;
}


bool enterPassword(unsigned long period) {
  unsigned long timeout = millis();
  String atCmd;
  bool pinEnt = false;
  tmpPassword = "";

  do {
    atCmd = sim800ReadDTMF();
    delay(100);
    if (atCmd.indexOf("+DTMF: 1") > -1) {
      tmpPassword += "1";
      Serial.print(F("1"));
    } else if (atCmd.indexOf("+DTMF: 2") > -1) {
      tmpPassword += "2";
      Serial.print(F("2"));
    } else if (atCmd.indexOf("+DTMF: 3") > -1) {
      tmpPassword += "3";
      Serial.print(F("3"));
    } else if (atCmd.indexOf("+DTMF: 4") > -1) {
      tmpPassword += "4";
      Serial.print(F("4"));
    } else if (atCmd.indexOf("+DTMF: 5") > -1) {
      tmpPassword += "5";
      Serial.print(F("5"));
    } else if (atCmd.indexOf("+DTMF: 6") > -1) {
      tmpPassword += "6";
      Serial.print(F("6"));
    } else if (atCmd.indexOf("+DTMF: 7") > -1) {
      tmpPassword += "7";
      Serial.print(F("7"));
    } else if (atCmd.indexOf("+DTMF: 8") > -1) {
      tmpPassword += "8";
      Serial.print(F("8"));
    } else if (atCmd.indexOf("+DTMF: 9") > -1) {
      tmpPassword += "9";
      Serial.print(F("9"));
    } else if (atCmd.indexOf("+DTMF: 0") > -1) {
      tmpPassword += "0";
      Serial.print(F("0"));
    } else if (atCmd.indexOf("+DTMF: #") > -1) {
      tmpPassword = "";
      Serial.print(F("#"));
      break;      
    } else if (atCmd.indexOf("NO CARRIER") > -1){ 
      break;
    }

    if (tmpPassword.length() == 4) pinEnt = true;
        

          
  } while (!pinEnt && (timeout+period) > millis());

  Serial.println();

  return pinEnt;
  
}

