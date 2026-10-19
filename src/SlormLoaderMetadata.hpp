#pragma once
#include "vendor/nlohmann/json.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <fstream>
namespace Slorm {
using Json=nlohmann::json;
inline constexpr const char* RuntimePackageVersion="0.6.1";
}
