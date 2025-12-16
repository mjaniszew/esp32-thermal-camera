#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_MLX90640.h>
#include <ArduinoJson.h>
#include <secrets.h> // Here store WiFi credentials and other secrets

Adafruit_MLX90640 mlx;
const int GRID_WIDTH = 32;
const int GRID_HEIGHT = 24;
const int DATA_SIZE = GRID_WIDTH * GRID_HEIGHT;
float frame[DATA_SIZE]; // buffer for full frame of temperatures

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

const char* HTML_CONTENT = R"rawliteral(
<!doctype html><html><head><title>ESP32 Thermal Camera</title><style>body { font-family: Arial, sans-serif; text-align: center; color: white; background: rgb(21, 21, 21) }
        #canvas-container { margin: 10px auto; border: 2px solid #333; width: 480px; height: 360px; }
        .temp-info { margin-right: 10px; display: inline }
        canvas { display: block; width: 100%}</style><script defer="defer" src="main.js"></script></head><body><div id="canvas-container"><canvas id="thermalCanvas" width="320" height="240"></canvas></div><div><p class="temp-info">min: <span id="minTemp">N/A</span> / max: <span id="maxTemp">N/A</span></p><p class="temp-info">|</p><label for="palette">Colors:</label> <select name="colors" id="palette"><option value="rainbow">Rainbow</option><option value="whitehot">White Hot</option><option value="nightvision">Nightvision</option><option value="iron">Iron</option></select></div><script>const wsAddr=`ws://${window.location.host}/ws`;let webSocket;const canvas=document.getElementById("thermalCanvas"),ctx=canvas.getContext("2d"),gridWidth=32,gridHeight=24,interpolationScale=10,pixelSize=1;function initWebsocket(){const t=new WebSocket(wsAddr);return t.onopen=function(){console.log("WebSocket connected")},t.onmessage=function(t){try{const e=JSON.parse(t.data);e.temperatures&&drawThermalMap(e.temperatures)}catch(t){console.error("Error parsing WebSocket message:",t)}},t.onclose=function(){console.log("WebSocket closed")},t.onerror=function(t){console.log("WebSocket error: "+t)},t}function temperatureToRainbow(t,e,o){let r=(t-e)/(o-e);return r=Math.max(0,Math.min(1,r)),`hsl(${240*(1-r)}, 100%, 50%)`}function temperatureToWhitehot(t,e,o){let r=(t-e)/(o-e);r=Math.max(0,Math.min(1,r));let a=100*Math.pow(r,1.5);return`hsl(${Math.round(0)}, ${Math.round(0)}%, ${Math.round(a)}%)`}function temperatureToNightvision(t,e,o){let r=(t-e)/(o-e);r=Math.max(0,Math.min(1,r));let a=270-250*r,n=60*Math.pow(r,1.5);if(n=Math.max(5,n),r>.9){n=60+50*(10*(r-.9)),n=Math.min(100,n)}return`hsl(${Math.round(a)}, 100%, ${Math.round(n)}%)`}function temperatureToIron(t,e,o){let r,a=(t-e)/(o-e);a=Math.max(0,Math.min(1,a));let n,i=100;if(a<=.5){r=270+90*(a/.5)}else if(a<.9){r=60*((a-.5)/.4)}else r=60;if(r%=360,n=65*Math.pow(a,1.5),n=Math.max(0,n),a>=.9){let t=10*(a-.9);n=65+35*t,i=100-100*t}return`hsl(${Math.round(r)}, ${Math.round(Math.max(0,i))}%, ${Math.round(Math.min(100,n))}%)`}const temperatureToPalette={rainbow:temperatureToRainbow,whitehot:temperatureToWhitehot,nightvision:temperatureToNightvision,iron:temperatureToIron};async function fetchSensorData(){try{const t=await fetch("/data"),e=await t.json();e&&e.temperatures&&drawThermalMap(e.temperatures)}catch(t){console.error("Error fetching data:",t)}}function interpolateTemperatures(t,e,o,r,a){if(t.length!==e*o)return console.error("Input data size does not match low resolution dimensions."),[];const n=e/r,i=o/a,c=new Array(r*a),l=(r,a)=>(r=Math.max(0,Math.min(e-1,Math.floor(r))),a=Math.max(0,Math.min(o-1,Math.floor(a))),t[a*e+r]);for(let t=0;t<a;t++)for(let a=0;a<r;a++){const h=a*n,s=t*i,m=Math.floor(h),u=Math.floor(s),d=Math.min(e-1,m+1),p=Math.min(o-1,u+1),M=h-m,f=s-u,w=(1-f)*((1-M)*l(m,u)+M*l(d,u))+f*((1-M)*l(m,p)+M*l(d,p));c[t*r+a]=w}return c}function drawThermalMap(t){if(768!==t.length)return void console.error("Data size mismatch.");const e=interpolateTemperatures(t,32,24,320,240),o=Math.min(...t),r=Math.max(...t);let a=temperatureToPalette.rainbow;try{document.getElementById("minTemp").textContent=o.toFixed(1),document.getElementById("maxTemp").textContent=r.toFixed(1)}catch{console.error("Error updating temperature display.")}try{const t=document.getElementById("palette").value||"rainbow";a=temperatureToPalette[t]||temperatureToPalette.rainbow}catch{console.error("Error getting color palette. Fallback to rainbow")}for(let t=0;t<240;t++)for(let n=0;n<320;n++){const i=a(e[32*t*10+n],o,r),c=320-n-1;ctx.fillStyle=i,ctx.fillRect(1*c,1*t,1,1)}}webSocket=initWebsocket(),webSocket||(fetchSensorData(),setInterval(fetchSensorData,1e3))</script></body></html>
)rawliteral";

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
    server.addHandler(&ws).addMiddleware([](AsyncWebServerRequest *request, ArMiddlewareNext next) {
        if (ws.count() > 1) {
            // if we have 2 clients or more, prevent the next one to connect
            request->send(503, "text/plain", "Server is busy");
        } else {
            next();
        }
    });
    ws.onEvent(onEvent);
    server.addHandler(&ws);
}

String getJsonData() {
    // Read sensor data
    mlx.getFrame(frame);

    // Prepare JSON response
    JsonDocument doc;
    JsonArray dataArray = doc["temperatures"].to<JsonArray>();
    for (float temp : frame) {
        dataArray.add(temp);
    }
    String output;
    serializeJson(doc, output);

    return output;
}

void sendDataToWsClients() {
    ws.textAll(getJsonData());
}

void setup() {
    Serial.begin(115200);

    // Setup MCU90640 thermal camera sensor with deafault I2C pins
    Serial.println("Setting up MCU90640 thermal sensor...");
    Wire.begin();
    mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire);
    mlx.setMode(MLX90640_INTERLEAVED);
    mlx.setResolution(MLX90640_ADC_16BIT);
    mlx.setRefreshRate(MLX90640_4_HZ);

    // Setup ESP32 WiFi Access Point
    Serial.println("Setting up AP...");
    WiFi.softAP(WIFI_SSID, WIFI_PASS);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Launch HTTP Server and Websocket
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", HTML_CONTENT);
    });
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", getJsonData());
    });
    server.begin();
    Serial.println("HTTP Server started.");
    
    initWebSocket();

    Serial.print("SSID: ");
    Serial.println(WIFI_SSID);
    Serial.print("Password: ");
    Serial.println(WIFI_PASS);
}

static uint32_t lastWS = 0;
static uint32_t deltaWS = 100;
static uint32_t lastHeap = 0;

void loop() {
    uint32_t now = millis();
    
    if (now - lastWS >= deltaWS) {
        if (ws.count() > 0){
            Serial.printf("Sending new frames to ws clients\n");
            sendDataToWsClients();
        }
        lastWS = millis();
    }

    if (now - lastHeap >= 2000) {
        Serial.printf("Connected ws clients: %u / %u total\n", ws.count(), ws.getClients().size());
        ws.cleanupClients(2);  // no more than 2 clients
        Serial.printf("Free heap: %" PRIu32 "\n", ESP.getFreeHeap());

        lastHeap = now;
    }

}
