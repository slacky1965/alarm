void setup() {
  Serial.begin(9600);
  SIM800.begin(9600);

  lastCheckSIM = lastCheckSimStatus = 0;
  firstStart = true;

  pinMode(PIN_RESET_SIM, OUTPUT);
  pinMode(PIN_POWER_ON, INPUT);
  sim800Reset();

  lastUpdate = millis();

  initDefaultConfig(&Config);

  if (!readConfig(&Config)) {
    firstCall = true;
    Serial.println(F("First Call!!!"));
  }

  Serial.println(F("Start Sim800L"));

}


