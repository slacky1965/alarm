void clearConfig(_config *config) {
  memset((void*)config, 0, sizeof(_config));
}

void initDefaultConfig(_config *config) {
  char *password = PASSWORD;
  clearConfig(config);
  strcpy(config->guestPassword, password);
  strcpy(config->userPassword, password);
}

void setConfig(_config *config) {
  memcpy((void*)&Config, (void*)config, sizeof(_config));
}

bool readConfig(_config *config) {
  int i;
  uint32_t datacrc;
  byte eBuf[sizeof(_config)];
  

//  EEPROM.begin(sizeof(_config)+sizeof(memcrc));


  for (i = EEPROM_START; i < EEPROM_START+sizeof(_config); i++) {
    eBuf[i] = EEPROM.read(i);
  }

  p_memcrc[0] = EEPROM.read(i++);
  p_memcrc[1] = EEPROM.read(i++);
  p_memcrc[2] = EEPROM.read(i++);
  p_memcrc[3] = EEPROM.read(i++);

  datacrc = crc_byte(eBuf, sizeof(_config));

  if (memcrc == datacrc) {
    memcpy((void*)&Config, (void*)eBuf, sizeof(_config));
    return true;
  }

  return false;
  
}

void saveConfig() {
  int i;
  byte eBuf[sizeof(_config)];

//  EEPROM.begin(sizeof(_config)+sizeof(memcrc));

  memcpy((void*)eBuf, (void*)&Config, sizeof(_config));

  for (i = EEPROM_START; i < EEPROM_START+sizeof(_config); i++) {
    EEPROM.write(i, eBuf[i]);
  }
  memcrc = crc_byte(eBuf, sizeof(_config));

  EEPROM.write(i++, p_memcrc[0]);
  EEPROM.write(i++, p_memcrc[1]);
  EEPROM.write(i++, p_memcrc[2]);
  EEPROM.write(i++, p_memcrc[3]);


//  EEPROM.commit();
}

