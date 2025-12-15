# esp32-thermal-camera

Simple ESP32 thermal camera project. Uses ESP32 and GY-MLX90640 or similar thermal camera working on I2C.

It serves WiFi Access Point, and after connecting to it you can access simple interface eg. on your phone to see video stream from the device.

Resolution and refresh rate are low due to MLX90640 limitations, but used interpolation improves image quality, and it's enough for simple use cases, like checking heat leaks at home.

# setup & development

- Add `src/secrets.h` file with your WiFi credentials
- After modifyiong client, it's required to move it's code and html (full or minimized) into `main.cpp` to be served. Remeber to delete mocked data and switch to real data fetching.