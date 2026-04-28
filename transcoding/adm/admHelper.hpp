// Copyright 2026 Cult-DSP
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <pugixml.hpp>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace cult {
namespace adm_helpers {

// Depth-first descendant traversal helper.
inline std::vector<pugi::xml_node> findAllDescendants(
    pugi::xml_node root,
    const char* elementName)
{
    std::vector<pugi::xml_node> result;
    pugi::xml_node cur = root.first_child();
    while (cur) {
        if (std::strcmp(cur.name(), elementName) == 0) {
            result.push_back(cur);
        }

        if (cur.first_child()) {
            cur = cur.first_child();
        } else if (cur.next_sibling()) {
            cur = cur.next_sibling();
        } else {
            while (cur && !cur.next_sibling() && cur != root)
                cur = cur.parent();
            if (cur == root || !cur) break;
            cur = cur.next_sibling();
        }
    }
    return result;
}

inline pugi::xml_node findFirstDescendant(pugi::xml_node root, const char* name) {
    pugi::xml_node cur = root.first_child();
    while (cur) {
        if (std::strcmp(cur.name(), name) == 0) return cur;
        if (cur.first_child()) {
            cur = cur.first_child();
        } else if (cur.next_sibling()) {
            cur = cur.next_sibling();
        } else {
            while (cur && !cur.next_sibling() && cur != root)
                cur = cur.parent();
            if (cur == root || !cur) break;
            cur = cur.next_sibling();
        }
    }
    return pugi::xml_node();
}

// Timecode parsing — mirrors _parse_timecode_to_seconds().
inline double parseTimecodeToSeconds(const char* tc) {
    if (!tc || tc[0] == '\0') return 0.0;
    int hours = 0, minutes = 0;
    double seconds = 0.0;
#ifdef _MSC_VER
#pragma warning(suppress: 4996)
#endif
    if (std::sscanf(tc, "%d:%d:%lf", &hours, &minutes, &seconds) == 3) {
        return hours * 3600.0 + minutes * 60.0 + seconds;
    }
    return 0.0;
}

inline double parseTimecodeToSeconds(const std::string& tc) {
    return parseTimecodeToSeconds(tc.c_str());
}

} // namespace adm_helpers
} // namespace cult
