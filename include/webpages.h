#pragma once
#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Garage Opener</title>
</head>
<body>
    <h1>Garage Opener Control</h1>
    <button id="toggle">Toggle</button>
    <script>
        document.getElementById("toggle").addEventListener("click", function() {
            fetch("/activate");
        });
    </script>
</body>
</html>
)rawliteral";

const char not_found_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Not Found</title>
</head>
<body>
    <h1>Page Not Found</h1>
    <p>The requested page was not found.</p>
    <p><a href="/">Go back to the homepage</a></p>
</body>
</html>
)rawliteral";