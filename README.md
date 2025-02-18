# ClownCar
Clown Car is an Arduino Nano ESP32 + OTG adapter that changes profiles for the RT4K based on gameID.

See it in action: https://youtu.be/ldbfFbKzjh8

### *** At the moment it takes about 35 seconds to "boot". Just be aware when you don't see any activity at first. ***

#### I believe you the only library you need to add is a fork of "EspUsbHost"
Go to: https://github.com/wakwak-koba/EspUsbHost 
 - Click the GREEN "<> Code" box and "Download ZIP"
 - In the Arudino IDE; goto "Sketch" -> "Include Library" -> "Add .ZIP Library"

<br />

------------

Edit the .ino file to include your gameID addresses and gameID to SVS profile matches.
```
Console consoles[] = {{"http://ps1digital.local/gameid",0,0,0}, // you can add more, but stay in this format
                      {"http://n64digital.local/gameid",0,0,0}
                      };

String gameDB[][2] = {{"00000000-00000000---00","7"}, // 7 is for S7_profilename.rt4
                      {"XSTATION","8"},               // XSTATION is the gameID from http://ps1digital.local/gameid
                      {"SCUS-94300","9"}};
```
WiFi is listed just below. Replace SSID and password with your network's. I believe it only works with 2.4Ghz WiFi.
```
wifiMulti.addAP("SSID","password");
```



