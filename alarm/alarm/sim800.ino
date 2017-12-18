bool sim800Check() {
  _response = sim800WriteCmd("AT+CPAS", DEFAULT_TIMEOUT);

  if (_response == "") simStatus = false;
  else {
    if (_response.indexOf("+CPAS: 0", 1) > -1 || _response.indexOf("+CPAS: 3", 1) > -1 || _response.indexOf("+CPAS: 4", 1) > -1) {
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

  sim800WriteCmd("AT", 3);
  sim800WriteCmd("ATE0", 3);                                  /* Echo mode off        */
  sim800WriteCmd("AT+DDET=1", 3);                             /* Set DTMF code        */
  sim800WriteCmd("AT+CLIP=1", 3);                             /* Set Caller ID        */
  sim800WriteCmd("AT+CMIC=0,"+(String)MIC_VOLUME, 3);         /* Set volume Mic input */
  sim800WriteCmd("AT+CMGF=1;&W", 3);                          /* Set TextMode for SMS */

  /* Delete all SMS after power on of system */
  if (firstStart) {
    sim800DeleteSMS(ALL);
    firstStart = false;
  }
}

String sim800WriteCmd(String cmd, unsigned long waiting) {
  String resp = "";
  Serial.println(cmd);
  SIM800.println(cmd);
  if (waiting > 0) {
    resp = sim800WaitResponse(waiting);
    Serial.println(resp);
  }
  return resp;
}

String sim800WaitResponse(unsigned long timeout) {
  String resp = "";
  unsigned long timeOld = millis();
  while (!SIM800.available() && !(millis() > timeOld + timeout*1000)) {
    delay(13);
  }
  if (SIM800.available()) {
    resp = SIM800.readString();
  } else {
    Serial.println(F("Timeout..."));
    simReset++;
  }
  return resp;
}

String sim800ReadDTMF() {
  int in;
  String cmd = "";
//  unsigned long timeStart = millis();
  SIM800.listen();
//  while (!SIM800.available() && (timeStart + 5) > millis()) {};
  while (SIM800.available()) {
    in = SIM800.read();
    cmd += char(in);
    delay(1);
  }
//  Serial.print(F("DTMF :")); Serial.println(cmd);
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

  String cmd;
  
  cmd = "AT+CMGDA=\"DEL ";

  switch (DEL) {
    case ALL:
      cmd += "ALL\"";
      break;
    case READ:
      cmd += "READ\"";
      break;
    case SENT:
      cmd += "SENT\"";
      break;
    default:
      cmd = "";
      break;
  }

  if (cmd != "") {
    sim800WriteCmd(cmd, DEFAULT_TIMEOUT);
  }
}


void sim800HangUp() {
  sim800WriteCmd("ATH", DEFAULT_TIMEOUT);
}

void sim800PlayTrack(_track track) {
  strcpy_P(fileName, (char*)pgm_read_word(&(msgName[track])));
  sim800WriteCmd(fileName, 0);
  delay(DEFAULT_DELAY_PLAY_TRACK*1000);
}

void sim800StopPlay(unsigned int t) {
  sim800WriteCmd("AT+CREC=5", t);
}

void sim800SendSMS(String phone, String message)
{
  sim800WriteCmd("AT+CMGS=\"" + phone + "\"", DEFAULT_TIMEOUT);
  sim800WriteCmd(message + "\r\n" + (String)((char)26), DEFAULT_TIMEOUT);
  deleteSentSMS = true;
}

String sim800IdxUnreadSMS() {
  String resp = sim800WriteCmd("AT+CMGL=\"REC UNREAD\",1", DEFAULT_TIMEOUT);
  return resp;
}

String sim800ReadSMS(int msgIndex) {
  String resp = sim800WriteCmd("AT+CMGR=" + (String)msgIndex + ",1", DEFAULT_TIMEOUT);
  return resp;
}

void sim800SetReadSMS(int msgIndex) {
  sim800WriteCmd("AT+CMGR=" + (String)msgIndex, DEFAULT_TIMEOUT);
}

void sim800RequestBalance() {
  sim800WriteCmd("AT+CUSD=1,\""+(String)BALANCE+"\"", DEFAULT_TIMEOUT);
}

void sim800Answer() {
  sim800WriteCmd("ATA", DEFAULT_TIMEOUT);
}

void sim800ParseSMS(String msg) {                                   // Парсим SMS

  /* SMS-commands 
   *  
   *  "1*[:1234]"           - alarm on
   *  "0*"                  - alarm off
   *  "100*[:1234]"         - balance inquiry
   *  "2*:2[:+7916xxxxxxx]" - add (or remove) a second phone number to the white list
   *  "2*:3[:+7916xxxxxxx]" - add (or remove) a third phone number to the white list
   *  "8*[:1234]"           - to send an SMS with the information about the system
   *  
   */
  
  String msgHeader  = "";
  String msgBody    = "";
  String msgPhone   = "";
  String password   = "";
  String newPhone   = "";
  bool phoneWhiteList = false;
  int idx, poz;

  msg = msg.substring(msg.indexOf("+CMGR: "));
  msgHeader = msg.substring(0, msg.indexOf("\r"));            // Выдергиваем телефон

  msgBody = msg.substring(msgHeader.length() + 2);
  msgBody = msgBody.substring(0, msgBody.lastIndexOf("OK"));  // Выдергиваем текст SMS
  msgBody.trim();

  int firstIndex = msgHeader.indexOf("\",\"") + 3;
  int secondIndex = msgHeader.indexOf("\",\"", firstIndex);
  msgPhone = msgHeader.substring(firstIndex, secondIndex);

  Serial.print(F("Phone: ")); Serial.println(msgPhone);        // Выводим номер телефона
  Serial.print(F("Message: ")); Serial.println(msgBody);     // Выводим текст SMS

  if (checkWhiteList(msgPhone)) {
    Serial.println(F("The phone from the whitelist"));
    phoneWhiteList = true;
  } else {
    Serial.println(F("Unknown phonenumber"));
    idx = msgBody.indexOf(":");
    if (idx > -1) {
      password = msgBody.substring(idx+1, idx+5);
      password.trim();
      Serial.print(F("password: ")); Serial.println(password);
      if (strcmp(password.c_str(), Config.guestPassword) != 0) return;
    } else {
      /* phone not present in white list and no password */
      return;
    }
  }

  if (msgBody.indexOf(CMD1) > -1) {
    Serial.print(F("Check command \"")); Serial.print(CMD1); Serial.println(F("\""));
  } else if (msgBody == CMD0) {
    Serial.print(F("Check command \"")); Serial.print(CMD0); Serial.println(F("\""));
  } else if (msgBody.indexOf(CMD2) > -1) {
    Serial.println(msgBody.substring(0, 2));
    if (msgBody.substring(0, 2) == CMD2) {
      Serial.print(F("Check command \"")); Serial.print(CMD2); Serial.println(F("\""));
      idx = msgBody.indexOf(":");
      if (idx > -1) {
        poz = msgBody.substring(idx+1, idx+2).toInt();
        if (poz == 2 || poz == 3) {
          idx = msgBody.indexOf(":", idx+1);
          if (idx > -1) {
            newPhone = msgBody.substring(idx+1);
            /* Invalid phone format */
            if (newPhone.length() != sizeof(Config.phoneWhiteList[poz-1])-1 || newPhone[0] != '+') return;
          }
          /* Stored in the variable password old number */
          password = Config.phoneWhiteList[poz-1];
          memset((void*)Config.phoneWhiteList[poz-1], 0, sizeof(Config.phoneWhiteList[poz-1]));
          if (newPhone != "") {
            strcpy(Config.phoneWhiteList[poz-1], newPhone.c_str());
            msgBody = "Added phone ";
            msgBody += poz;
            msgBody += " " + newPhone;
          } else {
            msgBody = "Delete phone ";
            msgBody += poz;
            msgBody += " " + password;
          }
          saveConfig();
          sim800SendSMS(msgPhone, msgBody);
        }
      }
    }
  } else if (msgBody.indexOf(CMD100) > -1) {
    Serial.print(F("Check command \"")); Serial.print(CMD100); Serial.println(F("\""));
    requestBalance = true;
    CallID = msgPhone;
  } else if (msgBody.indexOf(CMD9) > -1) {
    Serial.print(F("Check command \"")); Serial.print(CMD9); Serial.println(F("\""));
  }
}


