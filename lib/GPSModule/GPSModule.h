#pragma once

#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <TimeLib.h>

// Forward declaration
class GPSModule {
public:
    GPSModule(int rxPin, int txPin, uint32_t baudRate = 9600);
    void begin();
    void update();
    bool hasFix() const { return hasGPSFix; }
    bool isUsingWifi() const { return usingWifiLocation; }
    double getLatitude() const { return lastLat; }
    double getLongitude() const { return lastLng; }
    double getSpeed() const { return gps.speed.kmph(); }
    const char* getStatus() const;

private:
    void updatePosition(double lat, double lng, bool fromGPS);
    void tryWifiLocation();

    mutable TinyGPSPlus gps;
    HardwareSerial gpsSerial;
    
    bool hasGPSFix = false;
    bool usingWifiLocation = false;
    double lastLat = 0;
    double lastLng = 0;
    unsigned long lastFixTime = 0;
    unsigned long lastWifiTry = 0;
    
    const char* googleApiKey = "AIzaSyDH8gONfTO4jlCe_iWY69PP8FjyK8OQp7Q";
    const char* ntpServer = "pool.ntp.org";
}; 