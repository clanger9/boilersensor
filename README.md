# boilersensor
Boiler flow temperature compensator for Alpha InTec gas combi boiler

This code runs on an ESP32 dev board. It reads the ambient temperature from openweathermap.org and pretends to be a weather temperature compensation probe for an Alpha InTec gas combi boiler. I'm sure you can adapt it for other makes of boiler. I made this because it was cheaper/easier than fitting an external temperature probe.

Change the "xxxx" values in the code according to your setup so it can connect to your Wi-Fi and access the openweathermap.org API.
The external temperature probe parameters and compensation curves are as calculated in the spreadsheet.

The four digital outputs drive normally closed relay contacts. These relay contacts shoule be wired in series, with resistor values 15R, 33R, 60R and 120R across contacts 1-4 respectively. This forms a primitive 4-bit converter. Put a 733R (3x 2k2 in parallel) resisitor in series to mimic an Alpha InTec external temperature probe.

An optional themistor on the analogue input will allow you to monitor the flow temperature via the web interface.

Got to http://boiler.local to see the web interface.
Set the compensation number on the web page to match the setting on your boiler.
