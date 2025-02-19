# ClownCar
Clown Car is an Arduino Nano ESP32 + OTG adapter that changes profiles for the RT4K based on gameID. <br />

<img width="500" src=https://github.com/user-attachments/assets/4a560cfe-95f1-4e88-9f77-3f6dc569a9a4><br />



See it in action: https://youtu.be/ldbfFbKzjh8

### *** At the moment it takes about 35 seconds to "boot". Just be aware when you don't see any activity at first. ***

If you have multiple gameID consoles on when the Clown Car (CC) is booting, the console furthest up the "consoles" list wins. After that it keeps track of the order.

There are a multiple moving parts with this setup, and if you have issues, please use the "ClownCar_usb-only-test.ino". More info in the troublehshooting section at the end.

#### I believe the only library you need to add is a fork of "EspUsbHost"
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
WiFi is listed just below. Replace SSID and password with your network's. I believe it only works with 2.4Ghz WiFi. Make sure to not leave out the "" "" quotes.
```
wifiMulti.addAP("SSID","password");
```
<br />

## TroubleShooting ##
First make sure of the following:
 - Configured Wifi settings in .ino.
    - Make sure it's to a 2.4Ghz Wifi AP. I don't believe 5Ghz is supported on the Arduino.
 - Have at least 1 gameID to profile in the gameDB
 - Have at least 1 address in the consoles db that you can access with a web browser

 If you are sure of these settings, and it still does not work, try the following:
  - Configure your Arduino Nano ESP32 with the provided "ClownCar_usb-only-test.ino". This is configured to only load "remote profile 8".
  - Verify that everything is connected with your OTG adapter and has power.
  - Press the reset button on top of the Arduino and within a couple of seconds it should load remote profile 8.

    If this works, then there must be a wifi connectivity issue somewhere. 

    Here is a video of the "usb only test" being performed: https://www.youtube.com/watch?v=XP7OSW7X0DQ

