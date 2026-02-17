#pragma once

#include "types.h"
#include <string>
#include <vector>

namespace Utils {

// Print a commit summary to stdout (used by 'log' command)
void PrintCommit(const Commit& c, bool show_full_hash = false);

// Format bytes as a human-readable string e.g. "14.3 MB"
std::string FormatBytes(uintmax_t bytes);

// Trim whitespace from both ends of a string
std::string Trim(const std::string& s);

// Case-insensitive string compare
bool IEquals(const std::string& a, const std::string& b);

} // namespace Utils