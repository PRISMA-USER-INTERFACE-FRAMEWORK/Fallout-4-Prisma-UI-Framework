#pragma once

#include <cmath>
#include <iostream>
#include <string_view>

namespace DeterministicTests
{
    inline int failures = 0;

    inline void Expect(bool condition, std::string_view message)
    {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    }

    inline bool Near(float left, float right, float epsilon = 1.0e-4f)
    {
        return std::fabs(left - right) <= epsilon;
    }

    void TestWorldQuadGeometry();
    void TestBillboardAndValidation();
    void TestPointerNeutralGateAndEdges();
    void TestPointerCancellationAndMotion();
    void TestPointerRouting();
    void TestInputRegionPolicy();
}
