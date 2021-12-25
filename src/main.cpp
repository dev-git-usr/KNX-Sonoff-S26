#include <Arduino.h>
#include <knx.h>
#include <knx/bau57B0.h>  //KNX 

#ifdef ARDUINO_ARCH_ESP8266
#include <WiFiManager.h>
#endif

#define RELAYPIN 12  //GPIO 12
#define SWITCH_PIN 0 //GPIO 0
#define LED_PIN  13  //GPIO 13

#define DEVICENAME "knx-sonoff-s26"
// create named references for easy access to group objects
#define goSwitch knx.getGroupObject(1)
#define goBlock knx.getGroupObject(2)
#define goStatus knx.getGroupObject(3)


// callback from switch-GO
void switchCallback(GroupObject& go)
{
    Serial.println("GO-Switch");
    if (goBlock.value())
        return;
    
    bool value = goSwitch.value();
    digitalWrite(RELAYPIN, value);
    goStatus.value(value);
}

// callback previous to OTA-Update
void wifimgr_pre_ota() {
    knx.enabled(false);
}

WiFiManager wifiManager;    
WiFiManagerParameter custom_field; // global param ( for non blocking w params )
char custom_field_str[200];
void setup()
{
    Serial.begin(74880);
    randomSeed(millis());
    ArduinoPlatform::SerialDebug = &Serial;

    pinMode(RELAYPIN, OUTPUT);

    wifiManager.setHostname(DEVICENAME);
    wifiManager.setClass("invert"); // Dark Mode
    wifiManager.setCountry("DE"); //Default is CH maybe has different Channels

    wifiManager.setBreakAfterConfig(false);
    wifiManager.setTitle(DEVICENAME);
    wifiManager.setConfigPortalBlocking(false); //Not waiting for Connection/Configuration

    //Configure Menu 
    std::vector<const char *> menu = {"wifi","info","param","update","sep","restart", "erase"};
    wifiManager.setMenu(menu);

    wifiManager.autoConnect(DEVICENAME);
    wifiManager.startWebPortal(); //Enable Webpage after initial Configuration

    wifiManager.setPreOtaUpdateCallback(wifimgr_pre_ota);
    // read adress table, association table, groupobject table and parameters from eeprom
    knx.readMemory();

    if (knx.configured())
    {
      Serial.println("Device is Configured!");
      // register callback for reset GO
      goSwitch.callback(switchCallback);
      goSwitch.dataPointType(Dpt(1, 1));
      goBlock.dataPointType(Dpt(1, 3));
      goStatus.dataPointType(Dpt(1, 2));
    } else {
        Serial.println("Device is not Configured!");
    }
    
    // pin or GPIO the programming led is connected to. Default is LED_BUILTIN
    knx.ledPin(LED_PIN);
    // is the led active on HIGH or low? Default is LOW
    knx.ledPinActiveOn(LOW);
    // pin or GPIO programming button is connected to. Default is 0
    knx.buttonPin(SWITCH_PIN);
    knx.setButtonISRFunction(NULL);

    // start the framework. Will get wifi first.
    knx.start();

    //Setup Page for detailed Information
    //show only programmed Physical Address
    uint16_t area = (knx.individualAddress() & 0xF000) >> 12;
    uint16_t line = (knx.individualAddress() & 0x0F00) >> 8;
    uint16_t device = knx.individualAddress() & 0x00FF;
    sprintf(custom_field_str,"<br/><label for='customfieldid'>Device is programmed with ETS</label><br>Physical-Address: %d.%d.%d",area,line,device);

    new (&custom_field) WiFiManagerParameter(custom_field_str); // custom html input
  
    wifiManager.addParameter(&custom_field);
}

const int SHORT_PRESS_TIME = 1000; // 1000 ms
long pressedTime;
long releasedTime;
bool lastState = HIGH;
void loop() 
{
    // don't delay here to much. Otherwise you might lose packages or mess up the timing with ETS
    knx.loop();
    wifiManager.process(); 

    //Button Functions
    if(lastState == HIGH && digitalRead(SWITCH_PIN) == LOW) {
        pressedTime = millis();
        lastState = LOW;
    }
    releasedTime = millis();
    long pressDuration = releasedTime - pressedTime;
    if(lastState == LOW && (digitalRead(SWITCH_PIN) == HIGH || pressDuration > SHORT_PRESS_TIME)) {
        lastState = HIGH;
        if( pressDuration < SHORT_PRESS_TIME) {
            goSwitch.value(!goSwitch.value());
            switchCallback(goSwitch);
        } else {
            knx.toggleProgMode();
        }
    }
       

    // only run the application code if the device was configured with ETS
    if(!knx.configured())
        return;

    // nothing else to do.
}
