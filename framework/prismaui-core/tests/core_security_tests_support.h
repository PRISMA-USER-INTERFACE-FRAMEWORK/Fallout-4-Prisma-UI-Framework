#pragma once

#include <iostream>

namespace CoreSecurityTests {

    inline int failures = 0;

    inline void Check(bool value, const char* message) {
        if (!value) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    }

}  // namespace CoreSecurityTests
