#include "rotator/RotatorCoordinates.h"
#include <iostream>
#include <limits>
int main() {
    double out = 0;
    bool ok = mm::backendAzimuth(-45, 360, 0, 360, out) && out == 315;
    ok &= mm::backendAzimuth(315, 360, -180, 180, out) && out == -45;
    ok &= mm::backendAzimuth(405, 450, 0, 450, out) && out == 405;
    ok &= !mm::backendAzimuth(405, 450, 0, 360, out);
    ok &= !mm::backendAzimuth(std::numeric_limits<double>::quiet_NaN(), 360, 0, 360, out);
    std::cout << (ok ? "PASS" : "FAIL") << " rotator coordinate domains and overlap\n";
    return ok ? 0 : 1;
}
