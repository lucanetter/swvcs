#pragma once

// -------------------------------------------------------
// CommitEngine
// -------------------------------------------------------
// Orchestrates creating a commit:
//   1. Ask SwConnection to save the active doc
//   2. Read the file bytes and compute a SHA-256 hash
//   3. Copy the file into the blobs/ directory
//   4. Optionally capture a thumbnail
//   5. Write the Commit record via Repository
// -------------------------------------------------------

#include "types.h"

#include <filesystem>
#include <string>

class Repository;
class SwConnection;

class CommitEngine {
public:
    CommitEngine(Repository& repo, SwConnection& sw);

    // Create a new commit.
    // message: user-supplied description
    // capture_thumbnail: save a BMP preview alongside the snapshot
    Result Commit(const std::string& message, bool capture_thumbnail = true);

private:
    Repository&  repo_;
    SwConnection& sw_;

    // Compute SHA-256 hash of a file and return it as a hex string.
    // Returns empty string on failure.
    static std::string HashFile(const std::filesystem::path& path);

    // Copy src to dest (overwrites if exists).
    static Result CopyBlob(const std::filesystem::path& src,
                           const std::filesystem::path& dst);

    // Get current timestamp as ISO-8601 string.
    static std::string NowISO8601();

    // Get the OS username.
    static std::string GetAuthor();
};
