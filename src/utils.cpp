#include "utils.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace Utils {

void PrintCommit(const Commit& c, bool show_full_hash) {
    std::string display_hash = show_full_hash ? c.hash : c.hash.substr(0, 8);
    std::cout << "commit " << display_hash << "\n"
              << "Author:  " << c.author    << "\n"
              << "Date:    " << c.timestamp << "\n"
              << "File:    " << c.sw_meta.doc_path << " (" << c.sw_meta.doc_type << ")\n";

    if (c.sw_meta.mass > 0 || c.sw_meta.volume > 0) {
        std::cout << std::fixed << std::setprecision(4)
                  << "Mass:    " << c.sw_meta.mass   << " kg\n"
                  << "Volume:  " << c.sw_meta.volume << " m^3\n";
    }
    if (c.sw_meta.feature_count > 0) {
        std::cout << "Features:" << c.sw_meta.feature_count << "\n";
    }
    std::cout << "\n    " << c.message << "\n\n";
}

std::string FormatBytes(uintmax_t bytes) {
    const char* units[] = { "B", "KB", "MB", "GB" };
    double val = static_cast<double>(bytes);
    int i = 0;
    while (val >= 1024.0 && i < 3) { val /= 1024.0; ++i; }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << val << " " << units[i];
    return oss.str();
}

std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

bool IEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin(), [](char x, char y){
        return std::tolower(x) == std::tolower(y);
    });
}

} // namespace Utils