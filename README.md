# boilersensor
Boiler flow temperature compensator for Alpha InTec gas combi boiler

This code runs on an ESP32 dev board. It reads the ambient temperature from openweathermap.org and pretends to be a weather temperature compensation probe for an Alpha InTec gas combi boiler. I'm sure you can adapt it for other makes of boiler. I made this because it was cheaper/easier than fitting an external temperature probe.

Change the "xxxx" values in the code for your setup
The probe parameters are calculated in the spreadsheet

Got to http://boiler.local to see the web interface
Set the compensation number on the web page to match the setting on your boiler
