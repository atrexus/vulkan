#include <cstdint>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <windows.h>

namespace ImageUtils {
    uint32_t getChecksum(const char* path);

    // is successful
    bool fixImage(const std::string& filepath);
}