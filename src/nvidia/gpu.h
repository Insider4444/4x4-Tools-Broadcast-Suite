#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace fourbyfour {
struct Gpu {
    int ordinal{}, major{}, minor{}, driverApi{};
    std::uint64_t memory{};
    std::string name;
};
bool supportedArchitecture(const Gpu& gpu);
std::vector<Gpu> detectGpus();
Gpu selectGpu();
}
