/*
* RT4K ClownCar v0.000002
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

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <EspUsbHostSerial_FTDI.h> // https://github.com/wakwak-koba/EspUsbHost in order to have FTDI support for the RT4K usb serial port, this is the easist method.
                                  // Step 1 - Goto the github link above. Click the GREEN "<> Code" box and "Download ZIP"
                                  // Step 2 - In Arudino IDE; goto "Sketch" -> "Include Library" -> "Add .ZIP Library"


WiFiMulti wifiMulti;

uint16_t currentProf[2] = {1,0};  // first index: 0 = remote button profile,1 = SVS profiles. second index: profile number
unsigned long currentGameTime = 0;
unsigned long prevGameTime = 0;


class SerialFTDI : public EspUsbHostSerial_FTDI {
  public:
  String cprof = "null";
  String tcprof = "null";
  void task(void) override {
    EspUsbHost::task();
    if (this->isReady()) {
      esp_err_t err = usb_host_transfer_submit(this->usbTransfer_recv);
      if (err != ESP_OK && err != ESP_ERR_NOT_FINISHED && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGI("EspUsbHostSerial", "usb_host_transfer_submit() err=%x", err);
      }
      if(cprof != "null" && currentProf[0] == 0 && currentProf[1] != cprof.toInt()) {
        currentProf[1] = cprof.toInt();
        tcprof = "remote prof" + cprof + "\r";
        submit((uint8_t *)reinterpret_cast<const uint8_t*>(&tcprof[0]), tcprof.length());
      }
      else if(cprof != "null" && currentProf[0] == 1 && currentProf[1] != cprof.toInt()){
        currentProf[1] = cprof.toInt();
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
////////////////////
//    CONFIG     //
//////////////////
*/

Console consoles[] = {{"http://ps1digital.local/gameid",0,0,0}, // you can add more, but stay in this format
                      {"http://n64digital.local/gameid",0,0,0}
                      };

String gameDB[][2] = {{"00000000-00000000---00","7"}, // 7 would translate to a S7_profilename.rt4 named profile under RT4K-SDcard/profile/SVS/
                      {"XSTATION","8"},               // XSTATION is the gameID from http://ps1digital.local/gameid
                      {"SCUS-94300","9"}};

// WiFi config is just below

////////////////////////////////////////////////////////////////////////

const int consolelen = sizeof(consoles) / sizeof(consoles[0]); // length of consoles DB, used frequently
const int gameDBlen = sizeof(gameDB) / sizeof(gameDB[0]); // length of gameDB...

void setup(){

  wifiMulti.addAP("SSID","password"); // WiFi creds go here. MUST be a 2.4GHz WiFi AP, NOT 5GHz. Not supported by the Nano ESP32 unfortunately.
  WiFi.setHostname("clowncar.local"); // set hostname, call it whatever you like!

  usbHost.begin(115200); // leave at 115200 for RT4K connection

  pinMode(LED_GREEN, OUTPUT); // GREEN led lights up for 1 second when a SVS profile is sent
  pinMode(LED_BLUE, OUTPUT); // BLUE led is a WiFi activity. Long periods of blue means one of the gameID servers is not connecting.
  analogWrite(LED_GREEN,255);
  analogWrite(LED_BLUE,255);
  

}  // end of setup

void loop(){

  gameIDTimer(5000);  // 5000 == read gameID every 5 seconds

  usbHost.task();  // used for RT4K usb serial communications

}  // end of loop()

int fetchGameIDProf(String gameID){ // looks at gameDB for a gameID -> profile match, returns -1 if nothing found
  for(int i = 0; i < gameDBlen; i++){
    if(gameDB[i][0] == gameID){
      return gameDB[i][1].toInt();
      break;
   }
 }
 return -1;
}  // end of fetchGameIDProf()

void readGameID(){ // queries addresses in "consoles" array for gameIDs
  String payload = "";
  int result = 0;
  for(int i = 0; i < consolelen; i++){
    //result = 0;
    if((wifiMulti.run() == WL_CONNECTED)){ // wait for WiFi connection
      analogWrite(LED_BLUE,222);
      HTTPClient http;
      http.begin(consoles[i].Address);
      int httpCode = http.GET();             // start connection and send HTTP header
      if(httpCode > 0 || httpCode == -11){   // httpCode will be negative on error, let the read error slide...
        if(httpCode == HTTP_CODE_OK){        // console is healthy // HTTP header has been sent and Server response header has been handled
          payload = http.getString();        
          char arr[payload.length()+1]; // prepare MemCardPro check
          strcpy(arr,payload.c_str());
          
          if(arr[0]=='{'){ // Checking if something starts with {, as typically Memcard Pro devices do.
            char * MCPGID = strtok(arr, ","); // Split at ,
            for(int k = 0; k < 2; k++){
              if(k==1){ // After the first comma,
                MCPGID = strtok(NULL, ":"); // Continue splitting string at :
                MCPGID = strtok(NULL, ","); // Continue splitting string at ,
                break; // Break outta here
              }
            }           
            strcpy(MCPGID-1, MCPGID+1); // Remove leading space and ""
            MCPGID[strlen(MCPGID)-1]='\0'; // Terminate the string
            payload = MCPGID;
          }
          result = fetchGameIDProf(payload);
          consoles[i].On = 1;
          if(consoles[i].Prof != result && result != -1){ // gameID found for console, set as King, unset previous King, send profile change 
            consoles[i].Prof = result;
            consoles[i].King = 1;
            for(int j=0;j < consolelen;j++){
              if(i != j && consoles[j].King == 1)
                consoles[j].King = 0;
            }
            usendSVS(consoles[i].Prof);
          }
       } 
      } // end of if(httpCode > 0 || httpCode == -11)
      else{ // console is off, set attributes to 0, find a console that is On starting at the top of the gameID list, set as King, send profile
        consoles[i].On = 0;
        consoles[i].Prof = 0;
        if(consoles[i].King == 1){
          currentProf[1] = 0;
          usbHost.cprof = String(0);
          for(int k=0;k < consolelen;k++){

            if(i == k){
              consoles[k].King = 0;
              for(int l=0;l < consolelen;l++){
                if(consoles[l].On == 1){
                  consoles[l].King = 1;
                  usendSVS(consoles[l].Prof);
                  break;
                }
              }
            }
   
          } // end of for()
        } // end of if()      
      } // end of else()
    http.end();
    analogWrite(LED_BLUE, 255);
    }  // end of WiFi connection
  }
}  // end of readGameID()

void gameIDTimer(uint16_t gTime){
  currentGameTime = millis();  // Init timer
  if(prevGameTime == 0)       // If previous timer not initialized, do so now.
    prevGameTime = millis();
  if((currentGameTime - prevGameTime) >= gTime){ // If it's been longer than gIDTime, readGameID() and reset the timer.
    currentGameTime = 0;
    prevGameTime = 0;
    readGameID();
 }
}  // end of gameIDTimer()

void usendRBP(uint16_t num){ // send Remote Button Profile // not being used atm
  usbHost.cprof = String(num);
  currentProf[0] = 0; // 0 is Remote Button Profile
}

void usendSVS(uint16_t num){ // send SVS Profile
  usbHost.cprof = String(num);
  currentProf[0] = 1; // 1 is SVS profile
}
