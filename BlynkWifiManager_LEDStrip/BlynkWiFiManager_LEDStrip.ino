#include <FS.h>                   //this needs to be first, or it all crashes and burns...
//#define BLYNK_DEBUG           // Comment this out to disable debug and save space
//#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

char blynk_token[34] = "BLYNK_TOKEN";

int arcoiro = 0; //variavel seletora de efeito
int timer_id; //id do timer para rodar os efeitos
int R = 0; 
int G = 0;
int B = 0;
int deepSleep = 0; //define se entra ou não em deepsleep
int wait_time = 10; //delay dos efeitos

bool shouldSaveConfig = false; //flag for saving data

#include <BlynkSimpleEsp8266.h>
#include <SimpleTimer.h>
BlynkTimer timer_blynk;

#define NUMPIXELS 23
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, D9, NEO_RGB + NEO_KHZ800);

void saveConfigCallback () {  //callback notifying us of the need to save config
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup()
{
/////////////////////////START of TZAPU WifiManager Code///////////////////////////
  Serial.begin(115200);
  Serial.println();
  Serial.println("Mounting FS...");    //read configuration from FS json

  if (SPIFFS.begin()) {
    Serial.println("Mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("Reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("Opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(blynk_token, json["blynk_token"]);
        } else {
          Serial.println("Failed to load json config");
        }
      }
    }
  } else {
    Serial.println("Failed to mount FS");
  }

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 33);   // was 32 length
  
  Serial.println(blynk_token);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  wifiManager.setSaveConfigCallback(saveConfigCallback);   //set config save notify callback
  
  wifiManager.addParameter(&custom_blynk_token);   //add all your parameters here

  wifiManager.setTimeout(600);   // 10 minutes to enter data and then Wemos resets to try again.

  //fetches ssid and pass and tries to connect, if it does not connect it starts an access point with the specified name
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("jimts33", "12345678")) {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  Serial.println("Connected to WiFi");   //if you get here you have connected to the WiFi

  strcpy(blynk_token, custom_blynk_token.getValue());    //read updated parameters

  if (shouldSaveConfig) {      //save the custom parameters to FS
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["blynk_token"] = blynk_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("Failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
/////////////////////////END of TZAPU WifiManager Code///////////////////////////
  
  Blynk.config(blynk_token);
  Blynk.connect();
  timer_id = timer_blynk.setInterval(1000L, effectsTimer);
  timer_blynk.disable(timer_id);
  strip.begin();
  strip.show();
  
}

//efeitos precisam rodar varias vezes, então o timer faz isso pra gente
void effectsTimer()
{
  switch (arcoiro)
  {
    case 1: rainbowCycle(wait_time); break;
    case 2: rainbow(wait_time); break;
    case 3: snakeBounce(wait_time); break;
    case 4: snakeBounceHalf(wait_time);
  }

}

//Seletor de efeito
BLYNK_WRITE(V1)
{
  arcoiro = param.asInt();
  if(arcoiro >= 1)
  {
    timer_blynk.enable(timer_id);
    timer_blynk.restartTimer(timer_id);
    switch (arcoiro)
    {
      case 1: Blynk.setProperty(V1,"label","Rainbow Cycle"); break;
      case 2: Blynk.setProperty(V1,"label","Rainbow"); break;
      case 3: Blynk.setProperty(V1,"label","Snake Bounce"); break;
      case 4: Blynk.setProperty(V1,"label","Snake Bounce 2"); break;
    }
  }else
  {
    Blynk.setProperty(V1,"label","Sem Efeito");
    timer_blynk.disable(timer_id);
  }
}

//Toda vez que resetar, irá rodar cada Blynk_write
BLYNK_CONNECTED() {
    Blynk.syncAll();
}

//Ativa o deepsleep por 20s, deixando o módulo menos quente, mas desabilita efeitos
BLYNK_WRITE(V0)
{
  deepSleep = param.asInt();
  if(deepSleep == 1)
  ESP.deepSleep(20000000);
}

//Pega do widget zeRGBra, valores de 0-255 e muda a cor da fita
BLYNK_WRITE(V2)
{
  R = param[0].asInt();
  G = param[1].asInt();
  B = param[2].asInt();
  
  for(int i=0;i<NUMPIXELS;i++){
    strip.setPixelColor(i, strip.Color(R,G,B)); 
    strip.show(); 
  }

}

//Muda o tempo de delay dos efeitos
BLYNK_WRITE(V3)
{
  wait_time = param.asInt();
}



  void loop()
{
  Blynk.run(); // Initiates Blynk
  //timer.run(); // Initiates SimpleTimer  
  timer_blynk.run();
}

//////////////////ADAFRUIT NEOPIXEL FUNCTIONS//////////////////

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void snakeBounce(uint8_t wait) {
  uint16_t i, j;
  for(i=0; i<NUMPIXELS; i++){
    strip.setPixelColor(i, strip.Color(0.2*R,0.2*G,0.2*B));
    strip.show();
  }
  for(i=0; i<NUMPIXELS; i++){
    strip.setPixelColor(i, strip.Color(R,G,B));
    if(i>0){strip.setPixelColor(i-1,strip.Color(0.2*R,0.2*G,0.2*B));}
    strip.show();
    delay(wait);
  }
  for(j=NUMPIXELS-1; j>0; j--){
    strip.setPixelColor(j, strip.Color(R,G,B));
    if(j!=(NUMPIXELS-1)){strip.setPixelColor(j+1,strip.Color(0.2*R,0.2*G,0.2*B));}
    strip.show();
    delay(wait);
  }
  
}

void snakeBounceHalf(uint8_t wait) {
  uint16_t i, j;
  for(i=0, j=NUMPIXELS-1; i<NUMPIXELS; i++, j--){
    strip.setPixelColor(i, strip.Color(R,G,B));
    if(i>0){strip.setPixelColor(i-1,strip.Color(0.2*R,0.2*G,0.2*B));}
    strip.setPixelColor(j, strip.Color(R,G,B));
    if(j!=(NUMPIXELS-1)){strip.setPixelColor(j+1,strip.Color(0.2*R,0.2*G,0.2*B));}
    strip.show();
    delay(wait);
  }
  
}

void rainbow(uint8_t wait) {
  uint16_t i, j;
  if(wait>30){wait = 30;}
  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;
  if(wait>30){wait = 30;}
  for(j=0; j<256; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}


// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
