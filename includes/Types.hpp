#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>

class Location;

using StringMap   = std::map<std::string, std::string>;
using StringPair  = std::pair<std::string, std::string>;
using LocationMap = std::map<std::string, Location>;
using StringSet   = std::set<std::string>;
