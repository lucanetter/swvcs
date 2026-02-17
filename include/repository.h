#pragma once

// -------------------------------------------------------
// Repository
// -------------------------------------------------------
// Manages the .swvcs/ folder that lives next to your
// SolidWorks project files.  Responsible for:
//   - Initialising a new repo
//   - Reading / writing commit metadata (JSON)
//   - Tracking HEAD (the currently checked-out commit)
//   - Storing compressed file blobs
// -------------------------------------------------------

#include "types.h"
#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

class Repository {
public:
    // Initialise from an existing .swvcs directory or create one.
    // project_dir: the folder that contains your .SLDPRT / .SLDASM files.
    explicit Repository(const fs::path& project_dir);

    // Returns true if the repo was found / created successfully.
    bool IsValid() const { return valid_; }

    // -------------------------------------------------------
    // Commits
    // -------------------------------------------------------

    // Write a new commit record.  The blob (compressed file bytes)
    // must already have been saved by CommitEngine.
    Result SaveCommit(const Commit& c);

    // Load a commit by its hash prefix (at least 7 chars).
    Result LoadCommit(const std::string& hash_prefix, Commit& out);

    // Return all commits, newest first.
    std::vector<Commit> ListCommits();

    // -------------------------------------------------------
    // HEAD management
    // -------------------------------------------------------
    std::string GetHead() const;              // returns current commit hash
    Result SetHead(const std::string& hash);  // update HEAD pointer

    // -------------------------------------------------------
    // Blob storage paths
    // -------------------------------------------------------
    fs::path BlobPath(const std::string& hash) const;
    fs::path ThumbnailPath(const std::string& hash) const;

    // -------------------------------------------------------
    // Paths
    // -------------------------------------------------------
    fs::path Root()       const { return repo_root_; }
    fs::path CommitsDir() const { return repo_root_ / "commits"; }
    fs::path BlobsDir()   const { return repo_root_ / "blobs"; }

private:
    fs::path project_dir_;
    fs::path repo_root_;   // project_dir_ / ".swvcs"
    bool     valid_ = false;

    void Init();
};