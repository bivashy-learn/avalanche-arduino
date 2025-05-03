#include "GPSModule.h"
#include <LovyanGFX.hpp>

// Color definitions
#define TFT_BLACK 0x0000
#define TFT_WHITE 0xFFFF

GPSModule::GPSModule(int rxPin, int txPin, uint32_t baudRate) 
    : gpsSerial(1) {
    gpsSerial.begin(baudRate, SERIAL_8N1, rxPin, txPin);
}

void GPSModule::begin() {
    configTime(0, 0, ntpServer);
}

void GPSModule::update() {
    // Process GPS data
    while (gpsSerial.available()) {
        char c = gpsSerial.read();
        if (gps.encode(c)) {
            if (gps.location.isValid() && gps.location.isUpdated()) {
                updatePosition(gps.location.lat(), gps.location.lng(), true);
            }
        }
    }

    // If no GPS fix after 30 seconds, try WiFi location
    if (!hasGPSFix && (millis() - lastWifiTry > 60000 || lastWifiTry == 0)) {
        tryWifiLocation();
        lastWifiTry = millis();
    }
}

void GPSModule::updatePosition(double lat, double lng, bool fromGPS) {
    lastLat = lat;
    lastLng = lng;
    lastFixTime = millis();
    hasGPSFix = fromGPS;
    usingWifiLocation = !fromGPS;
}

void GPSModule::tryWifiLocation() {
    if (WiFi.status() != WL_CONNECTED) return;

    WiFiClientSecure client;
    client.setInsecure();
    
    HTTPClient http;
    String url = "https://www.googleapis.com/geolocation/v1/geolocate?key=" + String(GOOGLE_API_KEY);
    
    String jsonRequest = "{\"considerIp\":\"true\",\"wifiAccessPoints\":[";
    
    int networks = WiFi.scanNetworks();
    for (int i = 0; i < networks && i < 5; i++) {
        if (i > 0) jsonRequest += ",";
        jsonRequest += "{\"macAddress\":\"" + WiFi.BSSIDstr(i) + "\",";
        jsonRequest += "\"signalStrength\":" + String(WiFi.RSSI(i)) + "}";
    }
    jsonRequest += "]}";
    
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(jsonRequest);
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);
        
        double lat = doc["location"]["lat"];
        double lng = doc["location"]["lng"];
        updatePosition(lat, lng, false);
    }
    
    http.end();
}

const char* GPSModule::getStatus() const {
    if (hasGPSFix) return "GPS";
    if (usingWifiLocation) return "WiFi";
    return "Loading";
}