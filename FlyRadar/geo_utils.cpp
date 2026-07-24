#include "geo_utils.h"
#include "config.h"
#include <math.h>

namespace geo {

double distanceKm(double lat1, double lon1, double lat2, double lon2) {
    constexpr double r = 6371.0;
    const double p1 = radians(lat1);
    const double p2 = radians(lat2);
    const double dp = radians(lat2 - lat1);
    const double dl = radians(lon2 - lon1);
    const double s = sin(dp * 0.5);
    const double t = sin(dl * 0.5);
    const double a = s * s + cos(p1) * cos(p2) * t * t;
    const double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return r * c;
}

double bearingDeg(double lat1, double lon1, double lat2, double lon2) {
    const double p1 = radians(lat1);
    const double p2 = radians(lat2);
    const double dl = radians(lon2 - lon1);
    const double y = sin(dl) * cos(p2);
    const double x = cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dl);
    double brng = degrees(atan2(y, x));
    brng = fmod(brng + 360.0, 360.0);
    return brng;
}

double angleDiffDeg(double a, double b) {
    double d = fmod(fabs(a - b) + 180.0, 360.0) - 180.0;
    return fabs(d);
}

bool isBearingInSector(double bearing, double fromDeg, double toDeg) {
    // Sektor może "przechodzić" przez 0° (fromDeg=320, toDeg=30).
    if (fromDeg <= toDeg) {
        return bearing >= fromDeg && bearing <= toDeg;
    }
    return bearing >= fromDeg || bearing <= toDeg;
}

bool isBearingEast(double bearing) {
    return isBearingInSector(bearing, EAST_SECTOR_MIN_DEG, EAST_SECTOR_MAX_DEG);
}

} // namespace geo
