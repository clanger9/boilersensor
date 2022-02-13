const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Boiler flow temperature compensator</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: left;}
    h2 {font-size: 3.0rem;}
    p {font-size: 3.0rem;}
    body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
  </style>
</head>
<body>
<h2>Boiler flow temperature compensator</h2>
Selected temperature compensation: %COMPENSATION_CURVE% <br>
Ambient temperature: %EXTERNAL_TEMP% <br>
Target flow temperature: %TARGET_TEMP% <br>
Measured flow temperature: %MEASURED_TEMP% <br>
    
<script>function updateInput(element) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/update?compensation="+element.value, true);
  xhr.send();
}
</script>
</body>
</html>
)rawliteral";
