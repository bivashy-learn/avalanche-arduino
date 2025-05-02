#include <Arduino.h>
#include <WiFi.h>
#include <ILI9488_NOTOUCH_SPI.hpp>
#include <LovyanGFX.hpp>
#include <OpenStreetMap-esp32.h>
#include "GPSModule.h"

const char *ssid = "WIW";
const char *password = "WhateverIsWonderful";

LGFX display;
OpenStreetMap osm;
GPSModule gps(18, 17); // RX=18, TX=17

// Map parameters
double centerLatitude = 51.169392;
double centerLongitude = 71.449074;
int zoom = 14;
double currentLongitude = centerLongitude;
double currentLatitude = centerLatitude;
unsigned long lastPositionChange = 0;
const int POSITION_CHANGE_DELAY = 3000; // Delay in ms before changing position

// Joystick pins
const int JOYSTICK_X_PIN = 9;
const int JOYSTICK_Y_PIN = 10;

// Potentiometer pin for zoom control
const int ZOOM_POT_PIN = 11;

int cacheSize = 35;
int mapWidth = 400;
int mapHeight = 400;

// Joystick configuration
const int JOYSTICK_THRESHOLD = 500;
const int CENTER_MIN = 1500;
const int CENTER_MAX = 2500;

// Movement configuration
double moveStep = 0.01;
const double BASE_MOVE_STEP = 0.05;
const int SPRITE_MOVE_LIMIT = 80;
int spriteOffsetX = 0;
int spriteOffsetY = 0;

// Zoom configuration
const int MIN_ZOOM = 1;
const int MAX_ZOOM = 18;
int lastZoom = zoom;
int currentZoom = zoom;  // Separate variable for display
unsigned long lastZoomChange = 0;
const int ZOOM_CHANGE_DELAY = 3000; // Delay in ms before changing zoom

// Map sprite
LGFX_Sprite mapSprite(&display);

void fetchNewMap();
void moveSprite();
void updateStatusDisplay();
void updateMapCenter();
void showLoadingText();
void drawPositionMarker(LGFX_Sprite& sprite);
void updateZoom();

void setup() {
  Serial.begin(115200); 
  
  log_e("Total PSRAM: %d bytes", ESP.getPsramSize());
  log_e("WiFi connecting to %s", ssid);

  // Initialize input pins
  analogSetAttenuation(ADC_11db);
  pinMode(ZOOM_POT_PIN, INPUT);

  osm.resizeTilesCache(cacheSize);
  osm.setResolution(mapWidth, mapHeight);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(10);
    log_e(".");
  }

  log_e("\nWiFi connected");
  
  // Initialize GPS
  gps.begin();
  
  // Initialize map sprite
  display.begin();
  display.setRotation(1);
  display.setBrightness(110);
  gps.update();
  fetchNewMap();
}

void loop() {
  // Update GPS
  gps.update();
  
  // Update map center if GPS has fix or using WiFi
  if (gps.hasFix() || gps.isUsingWifi()) {
    updateMapCenter();
  }
  
  // Update zoom based on potentiometer
  updateZoom();
  
  // Read joystick inputs
  int xValue = analogRead(JOYSTICK_X_PIN);
  int yValue = analogRead(JOYSTICK_Y_PIN);
  bool positionChanged = false;

  if (xValue < (CENTER_MIN - JOYSTICK_THRESHOLD)) {
    spriteOffsetX += 2;
    positionChanged = true;
  } 
  else if (xValue > (CENTER_MAX + JOYSTICK_THRESHOLD)) {
    spriteOffsetX -= 2;
    positionChanged = true;
  }

  if (yValue < (CENTER_MIN - JOYSTICK_THRESHOLD)) {
    spriteOffsetY += 2;
    positionChanged = true;
  } 
  else if (yValue > (CENTER_MAX + JOYSTICK_THRESHOLD)) {
    spriteOffsetY -= 2;
    positionChanged = true;
  }

  if (positionChanged) {
    moveSprite();
  }

  // Update status display
  updateStatusDisplay();
}

void updateMapCenter() {
  double newLat = gps.getLatitude();
  double newLng = gps.getLongitude();
  
  // Only update if position has changed significantly and enough time has passed
  if ((abs(newLat - currentLatitude) > 0.0001 || abs(newLng - currentLongitude) > 0.0001) &&
      millis() - lastPositionChange > POSITION_CHANGE_DELAY) {
    currentLatitude = newLat;
    currentLongitude = newLng;
    lastPositionChange = millis();
    fetchNewMap();
  }
}

void showLoadingText() {
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextSize(3);
    display.setCursor(display.width()/2 - 60, display.height()/2 - 20);
    display.println("Loading...");
}

void drawPositionMarker(LGFX_Sprite& sprite) {
    const int centerX = sprite.width() / 2;
    const int centerY = sprite.height() / 2;
    const int radius = 5;
    
    // Draw outer circle
    sprite.drawCircle(centerX, centerY, radius, TFT_RED);
    // Draw inner circle
    sprite.fillCircle(centerX, centerY, radius/2, TFT_RED);
}

void fetchNewMap() {
  log_e("Fetching new map - Long: %.6f, Lat: %.6f, Zoom: %d", 
        currentLongitude, currentLatitude, zoom);
  
  spriteOffsetX = 0;
  spriteOffsetY = 0;
  centerLongitude = currentLongitude;
  centerLatitude = currentLatitude;

  // Show loading text
  showLoadingText();
  
  // Fetch map (this is blocking)
  if (osm.fetchMap(mapSprite, centerLongitude, centerLatitude, zoom)) {
    // Draw position marker
    drawPositionMarker(mapSprite);
    mapSprite.pushSprite(0, 0);
  } else {
    log_e("Failed to fetch map.");
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.setCursor(10, 10);
    display.println("Map load failed!");
  }
}

void moveSprite() {
    // Calculate maximum allowed movement
    int maxOffsetX = (mapWidth - display.width()) / 2;
    int maxOffsetY = (mapHeight - display.height()) / 2;
    
    // Constrain the offsets
    spriteOffsetX = constrain(spriteOffsetX, -maxOffsetX, maxOffsetX);
    spriteOffsetY = constrain(spriteOffsetY, -maxOffsetY, maxOffsetY);
    
    // Draw the sprite
    mapSprite.pushSprite(-spriteOffsetX, -spriteOffsetY);
}

void updateStatusDisplay() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 100) return; // Update more frequently for smooth animation
    lastUpdate = millis();

    // Create a small sprite for status
    LGFX_Sprite statusSprite(&display);
    statusSprite.createSprite(200, 80);
    statusSprite.fillScreen(TFT_BLACK);
    
    // Set text properties
    statusSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    statusSprite.setTextSize(2);
    
    // Draw speed
    char speedStr[10];
    sprintf(speedStr, "%.1f km/h", gps.getSpeed());
    statusSprite.drawString(speedStr, 5, 5);
    
    // Draw status
    const char* status = gps.getStatus();
    statusSprite.drawString(status, 5, 25);
    
    // Draw compact coordinates
    char coordStr[30];
    sprintf(coordStr, "%.4f,%.4f", gps.getLatitude(), gps.getLongitude());
    statusSprite.setTextSize(1);
    statusSprite.drawString(coordStr, 5, 45);
    
    // Draw zoom level
    char zoomStr[15];
    sprintf(zoomStr, "Zoom: %d", currentZoom);
    statusSprite.setTextSize(1);
    statusSprite.drawString(zoomStr, 5, 60);
    
    // Push to bottom left corner
    statusSprite.pushSprite(0, display.height() - 80);
    statusSprite.deleteSprite();
}

void updateZoom() {
    int potValue = analogRead(ZOOM_POT_PIN);
    // Map potentiometer value (0-4095) to zoom range (MIN_ZOOM to MAX_ZOOM)
    int newZoom = map(potValue, 0, 4095, MIN_ZOOM, MAX_ZOOM);
    
    // Update display zoom immediately
    currentZoom = newZoom;
    
    // Only change actual zoom if it's different and enough time has passed
    if (newZoom != lastZoom && millis() - lastZoomChange > ZOOM_CHANGE_DELAY) {
        zoom = newZoom;
        lastZoom = newZoom;
        lastZoomChange = millis();
        fetchNewMap();
    }
}