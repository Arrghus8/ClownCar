/*
* RT4K ClownCar v0.2b
* Copyright(C) 2025 @Donutswdad
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation,either version 3 of the License,or
*(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not,see <http://www.gnu.org/licenses/>.
*/

#include <Arduino_JSON.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <EspUsbHostSerial_FTDI.h> // https://github.com/wakwak-koba/EspUsbHost in order to have FTDI support for the RT4K usb serial port, this is the easist method.
                                   // Step 1 - Goto the github link above. Click the GREEN "<> Code" box and "Download ZIP"
                                   // Step 2 - In Arudino IDE; goto "Sketch" -> "Include Library" -> "Add .ZIP Library"

/*
////////////////////
//    OPTIONS    //
//////////////////
*/

bool const VGASerial = false;    // Use onboard TX1 pin to send Serial Commands to RT4K.

bool const S0_pwr = true;        // When all consoles defined below are off, S0_<whatever>.rt4 profile will load


bool const S0_gameID = true;     // When a gameID match is not found for a powered on console, S0_gameID_prof will load

int const  S0_gameID_prof = 0;    // SVS profile that loads when no matching gameID is found, if SO_gameID is set to true


//////////////////

uint16_t currentProf = 33333;  // current SVS profile number, set high initially
unsigned long currentGameTime = 0;
unsigned long prevGameTime = 0;


class SerialFTDI : public EspUsbHostSerial_FTDI {
  public:
  String cprof = "null";
  String tcprof = "null";
  virtual void task(void) override {
    EspUsbHost::task();
    if (this->isReady()) {
      esp_err_t err = usb_host_transfer_submit(this->usbTransfer_recv);
      if (err != ESP_OK && err != ESP_ERR_NOT_FINISHED && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGI("EspUsbHostSerial", "usb_host_transfer_submit() err=%x", err);
      }
      if(cprof != "null" && currentProf != cprof.toInt()){
        currentProf = cprof.toInt();
        analogWrite(LED_GREEN,222);
        tcprof = "SVS NEW INPUT=" + cprof + "\r";
        submit((uint8_t *)reinterpret_cast<const uint8_t*>(&tcprof[0]), tcprof.length());
        delay(1000);
        tcprof = "SVS CURRENT INPUT=" + cprof + "\r";
        submit((uint8_t *)reinterpret_cast<const uint8_t*>(&tcprof[0]), tcprof.length());
        analogWrite(LED_GREEN,255);

      }
    }
  }
};

SerialFTDI usbHost;

struct Console {
  String Address;
  int Prof;
  int On;
  int King;
};

/*
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//    CONFIG     //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
*/

Console consoles[] = {{"http://ps1digital.local/gameid",0,0,0}, // you can add more, but stay in this format
                      {"http://10.0.1.53/api/currentState",0,0,0},
                   // {"http://ps2digital.local/gameid",0,0,0}, // remove leading "//" to uncomment and enable ps2digital
                   // {"http://10.0.0.14/api/currentState",0,0,0}, // address format for MemCardPro. replace IP address with your MCP address
                      {"http://n64digital.local/gameid",0,0,0} // the last one in the list has no "," at the end
                      };

                                 // {"<GAMEID>","SVS PROFILE #"},
String gameDB[][2] = {{"00000000-00000000---00","7"}, // 7 is the "SVS PROFILE", would translate to a S7_<USER_DEFINED>.rt4 named profile under RT4K-SDcard/profile/SVS/
                      {"XSTATION","8"},               // XSTATION is the <GAMEID>
                      {"GM4E0100","505"},             // GameCube
                      {"3E5055B6-2E92DA52-N-45","501"}, // N64 MarioKart 64
                      {"635A2BFF-8B022326-N-45","502"}, // N64 Mario 64
                      {"DCBC50D1-09FD1AA3-N-45","503"}, // N64 Goldeneye 007
                      {"492F4B61-04E5146A-N-45","504"}, // N64 Wave Race 64
                      {"SLUS-00214","10"}, // PS1 Ridge Racer Revolution
                      {"SCUS-94300","9"}}; // PS1 Ridge Racer

// WiFi config is just below

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const int consolelen = sizeof(consoles) / sizeof(consoles[0]); // length of consoles DB, used frequently
const int gameDBlen = sizeof(gameDB) / sizeof(gameDB[0]); // length of gameDB...

void setup(){

  WiFi.begin("SSID","password"); // WiFi creds go here. MUST be a 2.4GHz WiFi AP. 5GHz is NOT supported by the Nano ESP32.
  WiFi.setHostname("clowncar.local"); // set hostname, call it whatever you like!
  usbHost.begin(115200); // leave at 115200 for RT4K connection
  if(VGASerial){
    Serial0.begin(9600);
    while(!Serial0){;}   // allow connection to establish before continuing
    Serial0.print(F("\r")); // clear RT4K Serial buffer
  }
  pinMode(LED_GREEN, OUTPUT); // GREEN led lights up for 1 second when a SVS profile is sent
  pinMode(LED_BLUE, OUTPUT); // BLUE led is a WiFi activity. Long periods of blue means one of the gameID servers is not connecting.
  analogWrite(LED_GREEN,255);
  analogWrite(LED_BLUE,255);
  
}  // end of setup

void loop(){

  gameIDTimer(2000);  // 2000 == read gameID every 2 seconds
  usbHost.task();  // used for RT4K usb serial communications

}  // end of loop()


int fetchGameIDProf(String gameID){ // looks at gameDB for a gameID -> profile match, returns -1 if nothing found
  for(int i = 0; i < gameDBlen; i++){
    if(gameDB[i][0] == gameID){
      return gameDB[i][1].toInt();
      break;
   }
  }
  if(S0_gameID){
    return S0_gameID_prof;
  }
  else{
    return -1;
  }
}  // end of fetchGameIDProf()

void readGameID(){ // queries addresses in "consoles" array for gameIDs
  String payload = "";
  int result = 0;
  for(int i = 0; i < consolelen; i++){
    if(WiFi.status() == WL_CONNECTED){ // wait for WiFi connection
      HTTPClient http;
      http.setConnectTimeout(2000); // give only 2 seconds per console to check gameID, is only honored for IP-based addresses
      http.begin(consoles[i].Address);
      analogWrite(LED_BLUE,222);
      int httpCode = http.GET();             // start connection and send HTTP header
      if(httpCode > 0 || httpCode == -11){   // httpCode will be negative on error, let the read error slide...
        if(httpCode == HTTP_CODE_OK){        // console is healthy // HTTP header has been sent and Server response header has been handled
          consoles[i].Address = replaceDomainWithIP(consoles[i].Address); // replace Domain with IP in consoles array. this allows setConnectTimeout to be honored
          payload = http.getString();        
          JSONVar MCPjson = JSON.parse(payload); // 
          if(JSON.typeof(MCPjson) != "undefined"){ // If the response is JSON, continue
            if(MCPjson.hasOwnProperty("gameID")){  // If JSON contains gameID, reset payload to it's value
              payload = (const char*) MCPjson["gameID"];
            }
          }
          result = fetchGameIDProf(payload);
          consoles[i].On = 1;
          if(consoles[i].Prof != result && result != -1){ // gameID found for console, set as King, unset previous King, send profile change 
            consoles[i].Prof = result;
            consoles[i].King = 1;
            for(int j=0;j < consolelen;j++){ // set previous King to 0
              if(i != j && consoles[j].King == 1)
                consoles[j].King = 0;
            }
            usbHost.cprof = String((consoles[i].Prof));
            if(VGASerial)sendSVS(consoles[i].Prof);
          }
       } 
      } // end of if(httpCode > 0 || httpCode == -11)
      else{ // console is off, set attributes to 0, find a console that is On starting at the top of the gameID list, set as King, send profile
        consoles[i].On = 0;
        consoles[i].Prof = 33333;
        if(consoles[i].King == 1){
          currentProf = 33333;
          usbHost.cprof = String(33333);
          for(int k=0;k < consolelen;k++){
            if(i == k){
              consoles[k].King = 0;
              for(int l=0;l < consolelen;l++){ // find next Console that is on
                if(consoles[l].On == 1){
                  consoles[l].King = 1;
                  usbHost.cprof = String((consoles[l].Prof));
                  if(VGASerial)sendSVS(consoles[l].Prof);
                  break;
                }
              }
            }
   
          } // end of for()
        } // end of if()
        int count = 0;
        for(int m=0;m < consolelen;m++){
          if(consoles[m].On == 0) count++;
        }
        if(count == consolelen && S0_pwr){
          usbHost.cprof = "0";
          if(VGASerial)sendSVS(0);
        }   
      } // end of else()
    http.end();
    analogWrite(LED_BLUE, 255);
    }  // end of WiFi connection
  }
}  // end of readGameID()

void gameIDTimer(unsigned long gTime){
  currentGameTime = millis();  // Init timer
  if(prevGameTime == 0)       // If previous timer not initialized, do so now.
    prevGameTime = millis();
  if((currentGameTime - prevGameTime) >= gTime){ // If it's been longer than gIDTime, readGameID() and reset the timer.
    currentGameTime = 0;
    prevGameTime = 0;
    readGameID();
 }
}  // end of gameIDTimer()

String replaceDomainWithIP(String input){
  String result = input;
  int startIndex = 0;
  while(startIndex < result.length()){
    int httpPos = result.indexOf("http://",startIndex); // Look for "http://"
    if (httpPos == -1) break;  // No "http://" found
    int domainStart = httpPos + 7; // Set the position right after "http://"
    int domainEnd = result.indexOf('/',domainStart);  // Find the end of the domain (start of the path)
    if(domainEnd == -1) domainEnd = result.length();  // If no path, consider till the end of the string
    String domain = result.substring(domainStart,domainEnd);
    if(!isIPAddress(domain)){ // If the domain is not an IP address, replace it
      IPAddress ipAddress;
      if(WiFi.hostByName(domain.c_str(),ipAddress)){  // Perform DNS lookup
        result.replace(domain,ipAddress.toString()); // Replace the Domain with the IP address
      }
    }
    startIndex = domainEnd;  // Continue searching after the domain
  } // end of while()
  return result;
} // end of replaceDomainWithIP()

bool isIPAddress(String str){
  IPAddress ip;
  return ip.fromString(str);  // Returns true if the string is a valid IP address
}

void sendSVS(uint16_t num){
  analogWrite(LED_GREEN,222);
  Serial0.print(F("SVS NEW INPUT="));
  Serial0.print(num);
  Serial0.println(F("\r"));
  delay(1000);
  Serial0.print(F("SVS CURRENT INPUT="));
  Serial0.print(num);
  Serial0.println(F("\r"));
  analogWrite(LED_GREEN,255);
}
