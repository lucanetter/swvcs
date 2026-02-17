#pragma once

#include <string>
#include <vector>
#include <cstdint>

// -------------------------------------------------------
// A single committed snapshot
// -------------------------------------------------------
struct Commit {
    std::string hash;           // SHA-256 of the snapshot content
    std::string message;        // user-provided description
    std::string timestamp;      // ISO-8601 e.g. "2025-02-17T14:32:00Z"
    std::string parent_hash;    // empty string if this is the first commit
    std::string author;         // machine username for now

    // SW-specific metadata captured at commit time
    struct SwMeta {
        std::string doc_path;           // original path of the .SLDPRT / .SLDASM
        std::string doc_type;           // "Part", "Assembly", "Drawing"
        double      mass         = 0;   // kg   (0 if not available)
        double      volume       = 0;   // m³   (0 if not available)
        double      surface_area = 0;   // m²   (0 if not available)
        int         feature_count = 0;
        std::string material;           // material name, e.g. "1060 Alloy" (parts only)
        double      bbox_x = 0;         // bounding box X extent, mm
        double      bbox_y = 0;         // bounding box Y extent, mm
        double      bbox_z = 0;         // bounding box Z extent, mm
        int         config_count = 0;   // number of configurations
        int64_t     blob_size_bytes = 0; // file size of the stored snapshot
    } sw_meta;
};

// -------------------------------------------------------
// Result of a SW connection attempt
// -------------------------------------------------------
enum class SwConnectStatus {
    OK,
    NotRunning,
    ComError,
    NoActiveDocument,
};

// -------------------------------------------------------
// Simple error wrapper used throughout
// -------------------------------------------------------
struct Result {
    bool        ok  = true;
    std::string err = "";

    static Result success()                      { return {true,  ""}; }
    static Result failure(const std::string& msg){ return {false, msg}; }
};