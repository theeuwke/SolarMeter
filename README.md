# SolarMeter
Documentation is stored in the 'docs' folder.

* Documentation on how to install the hardware is found in the (docs/hardwaremanual.pdf) file
* The software manual is found in docs/softwaremanual.pdf.
* An extra manual on setting up the libraries for this project can be found in [updating libraries](docs/Updating libraries for Solarmeter.pdf)

# ESP8266 Support
* Added quick'n'dirty basic support for ESP8266, tried to keep as much original code as possible
* Just select an ESP8266 compatible board
* Add SSID and PSK to userdefs.h
* Logging (and Debug) to Uart
* P1 Uart is moved to SwSerial

TODO:
* add OTA
* add WiFiManager
