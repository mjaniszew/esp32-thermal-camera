# Thermal Camera based on ESP32 and MLX90640, with Web Interface

Simple ESP32 thermal camera project. Based on:
- [SparkFun_MLX90640_Arduino_Example](https://github.com/sparkfun/SparkFun_MLX90640_Arduino_Example)
- [ESPAsyncWebServer examples](https://github.com/ESP32Async/ESPAsyncWebServer)

Uses ESP32 with Wifi and `GY-MLX90640` or similar thermal camera working on `I2C` and based on`MLX90640`.

It serves WiFi Access Point, and serves simple web interface where you can see video stream from the camera eg. on your mobile phone.

Resolution and refresh rate are low due to `MLX90640` limitations, although used interpolation improves image quality and it's enough for simple use cases like checking heat leaks at home. Currently using camera drives directly from Melexis as I've got better refresh/quality results using them, but `Adafruit MLX90640` will work as well (easier to use, but got worse results there).

![Web Interface Preview](preview.jpg)

It utilizes websockets for communication, and in case of failure it pulls data via `GET` requests from `/data` endpoint as a fallback.

# Current features

- 4Hz refresh rate, should produce up to 4 frames per second over websocket connection
- Web interface with video stream and basic options
- It shows min and max temperatures registered on the screen
- Basic color palettes to choose, based on popular ones found in some industry cameras like: Rainbow, White Hot, Iron-like, etc.

# Modules/libs used

- MLX90640 driver directly from Melexis for thermal camera control
- WiFi for AP setup
- ESPAsyncWebServer for websocket and server communication
- ArduinoJson for json data handling

# Setup & Development

- Add `include/secrets.h` file with your WiFi credentials
- Bu default Web interface is served on `192.168.4.1` when connected to ESP32 AP. You can change this in `src/main.cpp`
- Web client with dummy data server can be found in `./web-client` folder
- After modifying client in `./web-client/src/index.html`, it's required to move its code and html (full or minimized) into `main.cpp` to be served on project build. There's build task which will minimize it for production
- Compile for ESP32 Dev Kit board, even if other ESP32 with WiFi is being used for final device
- Built used Platform.io. If you're using something else, remember to inlcude folders `include` and `libs` during compilation