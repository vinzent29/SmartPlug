#include <ESP8266WiFi.h>

#define filterSamples   13              // filterSamples should  be an odd number, no smaller than 3

int digitalSmooth(int rawIn, int *sensSmoothArray);
