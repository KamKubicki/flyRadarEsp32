#pragma once

#include <Arduino.h>

namespace geo {

double distanceKm(double lat1, double lon1, double lat2, double lon2);
double bearingDeg (double lat1, double lon1, double lat2, double lon2);
double angleDiffDeg(double a, double b);
bool   isBearingInSector(double bearing, double fromDeg, double toDeg);
bool   isBearingEast   (double bearing);

} // namespace geo
