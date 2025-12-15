#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_MLX90640.h>
#include <ArduinoJson.h>
#include <secrets.h> // Here store WiFi credentials and other secrets

Adafruit_MLX90640 mlx;
const int GRID_WIDTH = 32;
const int GRID_HEIGHT = 24;
const int DATA_SIZE = GRID_WIDTH * GRID_HEIGHT;
float frame[DATA_SIZE]; // buffer for full frame of temperatures

WebServer server(80);
IPAddress local_IP(192, 168, 2, 1);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);

const char* HTML_CONTENT = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Thermal Map</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; color: white; background: rgb(21, 21, 21) }
        #canvas-container { margin: 10px auto; border: 2px solid #333; width: 480px; height: 360px; }
        .temp-info { margin-right: 10px; display: inline }
        canvas { display: block; width: 100%}
    </style>
</head>
<body>
    <div id="canvas-container">
        <canvas id="thermalCanvas" width="320" height="240"></canvas>
    </div>
    <div>
        <p class="temp-info">min: <span id="minTemp">N/A</span> / max: <span id="maxTemp">N/A</span></p>
        <p class="temp-info">|</p>
        <label for="palette">Colors:</label>
        <select name="colors" id="palette">
            <option value="rainbow">Rainbow</option>
            <option value="arctic">Arctic</option>
            <option value="nightvision">Nightvision</option>
            <option value="fusion">Fusion</option>
        </select>
    </div>

    <script>
        const canvas = document.getElementById('thermalCanvas');
        const ctx = canvas.getContext('2d');
        const gridWidth = 32;
        const gridHeight = 24;
        const interpolationScale = 10;
        const pixelSize = 1;

        function temperatureToRainbow(value, minTemp, maxTemp) {
            // Normalize the value (0 to 1)
            let normalized = (value - minTemp) / (maxTemp - minTemp);
            normalized = Math.max(0, Math.min(1, normalized)); // Clamp between 0 and 1

            // Simple blue (cold/low) to red (hot/high) gradient
            let hue = (1 - normalized) * 240; // 240 is blue, 0 is red (HSL: H value)
            return `hsl(${hue}, 100%, 50%)`;
        }

        function temperatureToArctic(temperature, minTemp, maxTemp) {
            
            // Normalize the temperature to a 0.0 to 1.0 range (tNorm)
            let tNorm = (temperature - minTemp) / (maxTemp - minTemp);
            tNorm = Math.max(0.0, Math.min(1.0, tNorm)); // Clamp to [0, 1]
            
            // HUE (H): Fixed, since we are using 0% saturation.
            const hue = 0; 
            
            // SATURATION (S): Set to 0% to ensure the color is achromatic (grayscale).
            const saturation = 0;
            
            // LIGHTNESS (L): Maps directly from 0% (Black) to 100% (White).
            let lightness = Math.pow(tNorm, 1.5) * 100;

            // Final clamping and rounding
            const H = Math.round(hue);
            const S = Math.round(saturation);
            const L = Math.round(lightness); 

            return `hsl(${H}, ${S}%, ${L}%)`;
        }

        function temperatureToNightvision(temperature, minTemp, maxTemp) {
            // Normalize the temperature to a 0.0 to 1.0 range (tNorm)
            let tNorm = (temperature - minTemp) / (maxTemp - minTemp);
            
            // Clamp the value to ensure it stays within [0, 1]
            tNorm = Math.max(0.0, Math.min(1.0, tNorm));
            
            // HUE (H): Transition from Purple (270) to Yellow (60).
            // The range 270 - 60 = 210 degrees.
            let hue = 270 - (250 * tNorm); 
            
            // LIGHTNESS (L): Key for Black -> Bright Yellow transition.
            // Use a non-linear curve for high contrast (low temperatures stay very dark).
            // pow(tNorm, 1.5) ensures low tNorm values result in very low L.
            let lightness = Math.pow(tNorm, 1.5) * 60; 
            
            // Ensure minimum Lightness is 5% to avoid pure black unless tNorm is exactly 0
            lightness = Math.max(5, lightness); 

            // --- Adjustments for White-Hot Core (tNorm > 0.9) ---
            // This creates the very bright, desaturated core seen in the eyes/mouth.
            if (tNorm > 0.9) {
                // Calculate the progress within the white-hot zone (0 to 1)
                const whiteHotProgress = (tNorm - 0.9) * 10; 
                lightness = 60 + (50 * whiteHotProgress);
                lightness = Math.min(100, lightness);
            }

            const H = Math.round(hue);
            const L = Math.round(lightness);

            return `hsl(${H}, 100%, ${L}%)`;
        }

        function temperatureToFusion(temperature, minTemp, maxTemp) {
            
            // Normalize the temperature to a 0.0 to 1.0 range (tNorm)
            let tNorm = (temperature - minTemp) / (maxTemp - minTemp);
            tNorm = Math.max(0.0, Math.min(1.0, tNorm)); // Clamp to [0, 1]

            let hue;
            let saturation = 100;
            let lightness;

            // --- HUE (H) Mapping ---
            // Total desired range is 270 (Purple) -> 360/0 (Red) -> 60 (Yellow).
            // This is 90 degrees (270 to 360) + 60 degrees (0 to 60) = 150 degrees of change.
            
            // We will use a segmented approach to control the color distribution:
            // 0.0 to 0.5: Purple to Red (270 -> 360/0)
            // 0.5 to 0.9: Red to Yellow (360/0 -> 60)
            
            if (tNorm <= 0.5) {
                let segmentNorm = tNorm / 0.5;
                hue = 270 + (90 * segmentNorm); 
            } else if (tNorm < 0.9) {
                let segmentNorm = (tNorm - 0.5) / 0.4;
                hue = 60 * segmentNorm;
            } else {
                hue = 60;
            }
            hue = hue % 360;
            lightness = Math.pow(tNorm, 1.5) * 65; 
            lightness = Math.max(0, lightness); 
            
            if (tNorm >= 0.9) {
                let whiteHotProgress = (tNorm - 0.9) * 10;
                lightness = 65 + (35 * whiteHotProgress);
                saturation = 100 - (100 * whiteHotProgress);
            }
            
            const H = Math.round(hue);
            const S = Math.round(Math.max(0, saturation));
            const L = Math.round(Math.min(100, lightness)); 

            return `hsl(${H}, ${S}%, ${L}%)`;
        }

        const temperatureToPalette = {
          "rainbow": temperatureToRainbow,
          "arctic": temperatureToArctic,
          "nightvision": temperatureToNightvision,
          "fusion": temperatureToFusion
        };

        async function fetchSensorData() {
            try {
                const response = await fetch('/data');
                const data = await response.json();

                if (data && data.temperatures) {
                    drawThermalMap(data.temperatures);
                }
            } catch (error) {
                console.error("Error fetching data:", error);
            }
        }

        function interpolateTemperatures(lowResTemperatures, lowWidth, lowHeight, highWidth, highHeight) {
            
            // Ensure input size matches the expected dimensions
            if (lowResTemperatures.length !== lowWidth * lowHeight) {
                console.error("Input data size does not match low resolution dimensions.");
                return [];
            }

            // Scaling factors (should be 10 for both)
            const xScale = lowWidth / highWidth;
            const yScale = lowHeight / highHeight;

            const highResTemperatures = new Array(highWidth * highHeight);

            // Helper to safely get the temperature from the low-res array
            const getLowResTemp = (x, y) => {
                // Clamp coordinates to stay within bounds
                x = Math.max(0, Math.min(lowWidth - 1, Math.floor(x)));
                y = Math.max(0, Math.min(lowHeight - 1, Math.floor(y)));
                return lowResTemperatures[y * lowWidth + x];
            };

            for (let y = 0; y < highHeight; y++) {
                for (let x = 0; x < highWidth; x++) {
                    
                    // 1. Map the high-res coordinate (x, y) back to the low-res coordinate system (x_prime, y_prime)
                    const x_prime = x * xScale;
                    const y_prime = y * yScale;

                    // 2. Find the four surrounding integer coordinates (Q11, Q21, Q12, Q22)
                    const x1 = Math.floor(x_prime);
                    const y1 = Math.floor(y_prime);
                    const x2 = Math.min(lowWidth - 1, x1 + 1);
                    const y2 = Math.min(lowHeight - 1, y1 + 1);

                    // 3. Calculate the fractional distance (delta_x, delta_y) within the cell [0, 1]
                    // This is the weight for the right/bottom points.
                    const delta_x = x_prime - x1;
                    const delta_y = y_prime - y1;

                    // 4. Retrieve the temperatures from the four surrounding points
                    const Q11 = getLowResTemp(x1, y1); // Top-Left
                    const Q21 = getLowResTemp(x2, y1); // Top-Right
                    const Q12 = getLowResTemp(x1, y2); // Bottom-Left
                    const Q22 = getLowResTemp(x2, y2); // Bottom-Right

                    // 5. Apply Bilinear Interpolation formula (weighted average)
                    // Interpolate along the X-axis first: R1 and R2
                    const R1 = (1 - delta_x) * Q11 + delta_x * Q21; // Interpolation between Q11 and Q21
                    const R2 = (1 - delta_x) * Q12 + delta_x * Q22; // Interpolation between Q12 and Q22

                    // Interpolate along the Y-axis: P (the final temperature)
                    const P = (1 - delta_y) * R1 + delta_y * R2;

                    // Store the interpolated temperature in the high-res array
                    highResTemperatures[y * highWidth + x] = P;
                }
            }

            return highResTemperatures;
        }

        function drawThermalMap(temperatures) {
            if (temperatures.length !== gridWidth * gridHeight) {
                console.error("Data size mismatch.");
                return;
            }

            const interpolatedTemperatures = interpolateTemperatures(temperatures, gridWidth, gridHeight, gridWidth * interpolationScale, gridHeight * interpolationScale);

            // Find min/max for color scaling
            const minTemp = Math.min(...temperatures);
            const maxTemp = Math.max(...temperatures);
            let paletteHandler = temperatureToPalette['rainbow'];

            try {
              document.getElementById('minTemp').textContent = minTemp.toFixed(1);
              document.getElementById('maxTemp').textContent = maxTemp.toFixed(1);
            } catch {
              console.error("Error updating temperature display.");
            }

            try {
              const palette = document.getElementById('palette').value || 'rainbow';
              paletteHandler = temperatureToPalette[palette] || temperatureToPalette['rainbow'];
            } catch {
              console.error("Error getting color palette. Fallback to rainbow");
            }

            for (let y = 0; y < gridHeight * interpolationScale; y++) {
                for (let x = 0; x < gridWidth * interpolationScale; x++) {
                    const index = y * gridWidth * interpolationScale + x;
                    const temp = interpolatedTemperatures[index];
                    const color = paletteHandler(temp, minTemp, maxTemp);
                    const xPos = gridWidth * interpolationScale - x - 1; // flip horizontally
                    ctx.fillStyle = color;
                    ctx.fillRect(xPos * pixelSize, y * pixelSize, pixelSize, pixelSize);
                }
            }
        }

        // Initialize and set the update interval
        fetchSensorData(); // Run once immediately
        setInterval(fetchSensorData, 1000); // Call every 1 second

    </script>
</body>
</html>
)rawliteral";

void handleRoot() {
    server.send(200, "text/html", HTML_CONTENT);
}

void handleData() {
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

    server.send(200, "application/json", output);
}

void setup() {
    Serial.begin(115200);

    // Setup MCU90640 thermal camera sensor with deafault I2C pins
    Serial.println("Setting up MCU90640 thermal sensor...");
    Wire.begin();
    mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire);
    mlx.setMode(MLX90640_INTERLEAVED);
    mlx.setResolution(MLX90640_ADC_16BIT);
    mlx.setRefreshRate(MLX90640_2_HZ);

    // Setup ESP32 WiFi Access Point
    Serial.println("Setting up AP...");
    WiFi.softAP(WIFI_SSID, WIFI_PASS);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Launch HTTP Server
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.begin();
    Serial.println("HTTP Server started.");

    Serial.println("Connect to the network:");
    Serial.print("SSID: ");
    Serial.println(WIFI_SSID);
    Serial.print("Password: ");
    Serial.println(WIFI_PASS);
}

void loop() {
    server.handleClient();
}