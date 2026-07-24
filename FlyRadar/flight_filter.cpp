#include "flight_filter.h"
#include "geo_utils.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

namespace {

bool courseMatch(double track, double runwayHeading) {
    if (isnan(track)) {
        return false;
    }
    return geo::angleDiffDeg(track, runwayHeading) <= COURSE_TOLERANCE_DEG;
}

FlightKind classifyOperation(const AircraftState &a, char *outOp, size_t outOpSize) {
    if (isnan(a.trackDeg) || isnan(a.verticalRateFpm)) {
        return FlightKind::Unknown;
    }

    if (courseMatch(a.trackDeg, RWY_HEADING_25) && a.verticalRateFpm < VR_DESCEND_LANDING_FPM) {
        snprintf(outOp, outOpSize, "Landing from east");
        return FlightKind::Arrival;
    }
    if (courseMatch(a.trackDeg, RWY_HEADING_25) && a.verticalRateFpm > VR_CLIMB_TAKEOFF_FPM) {
        snprintf(outOp, outOpSize, "Departing west");
        return FlightKind::Departure;
    }
    if (courseMatch(a.trackDeg, RWY_HEADING_07) && a.verticalRateFpm < VR_DESCEND_LANDING_FPM) {
        snprintf(outOp, outOpSize, "Landing from west");
        return FlightKind::Arrival;
    }
    if (courseMatch(a.trackDeg, RWY_HEADING_07) && a.verticalRateFpm > VR_CLIMB_TAKEOFF_FPM) {
        snprintf(outOp, outOpSize, "Departing east");
        return FlightKind::Departure;
    }
    return FlightKind::Transit;
}

} // namespace

ClassificationResult flightFilterClassify(const AircraftState &a) {
    ClassificationResult r;

    if (isnan(a.altitudeFt)) {
        return r;
    }
    if (a.altitudeFt > MAX_ALTITUDE_FT || a.altitudeFt < MIN_ALTITUDE_FT) {
        return r;
    }
    if (a.distanceKm > MAX_HOME_DISTANCE_KM) {
        return r;
    }
    if (!isnan(a.airportDistanceKm) && a.airportDistanceKm > MAX_AIRPORT_DISTANCE_KM) {
        return r;
    }
    if (!isnan(a.bearingDeg)) {
        r.isEastVisible = geo::isBearingEast(a.bearingDeg);
    }

    r.kind = classifyOperation(a, r.operation, sizeof(r.operation));
    // Lądowanie na EPKK = zawsze widoczne (pilot też widzi niezależnie od kierunku)
    if (r.kind == FlightKind::Arrival) r.isEastVisible = true;
    r.accepted = true;
    return r;
}

bool flightFilterIsCandidateRelevant(const ClassificationResult &r,
                                    bool showArrivals,
                                    bool showDepartures,
                                    VisibilitySide side) {
    if (!r.accepted) {
        return false;
    }
    if (r.kind == FlightKind::Unknown) {
        return false;
    }
    if (r.kind == FlightKind::Arrival   && !showArrivals)   return false;
    if (r.kind == FlightKind::Departure && !showDepartures) return false;
    if (side == VisibilitySide::EastOnly && !r.isEastVisible) {
        return false;
    }
    return true;
}
