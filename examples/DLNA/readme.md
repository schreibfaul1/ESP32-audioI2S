# DLNA
 DLNA servers are available in many home networks. Many Internet routers (e.g. Fritzbox) have an integrated DLNA service. It is also easy to set up a own DLNA server (e.g. miniDLNA on a Raspberry Pi). The SoapESP32 library https://github.com/yellobyte/SoapESP32 used here automatically recognizes the DLNA servers available in the home network. Thanks to yellobyte for this library. Since the SW is too extensive for a sketch, I have published a complete project here. Simply download the repository and unzip the DLNA folder. You can open the project with PlatformIO. Change the access data and, if necessary, the GPIOs in main.cpp. If a DLNA server was detected, its content will be displayed in the browser. If an audio file is selected, the playback process begins using the audioI2S library.
 <br>
 Webpage
![Webpage](https://github.com/schreibfaul1/ESP32-audioI2S/blob/master/examples/DLNA/additional_info/DLNA_web.jpg)
