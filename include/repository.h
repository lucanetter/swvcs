#pragma once

// -------------------------------------------------------
// Repository
// -------------------------------------------------------
// Manages the .swvcs/ folder that lives next to your
// SolidWorks project files.  Responsible for:
//   - Initialising a new repo
//   - Reading / writing commit metadata (SQLite database)
//   - Tracking HEAD (stored in the same database)
//   - Providing paths for binary blobs and thumbnails
//     (large files stay on disk — not in the DB)
//
// Storage layout:
//   .swvcs/
//     swvcs.db       ← SQLite database (commits + config)
//     blobs/         ← raw .SLDPRT snapshots (unchanged)
//     thumbs/        ← 256x256 BMP previews  (unchanged)
// -------------------------------------------------------

#include "types.h"
#include <vector>
#include <string>
#include <filesystem>
#include <memory>

// Forward-declare SQLite::Database so the SQLiteCpp headers
// are only compiled in repository.cpp, not everywhere.
namespace SQLite { class Database; }

namespace fs = std::filesystem;

class Repository {
public:
    // Open an existing repo or create a new one.
    // project_dir: the folder that contains your .SLDPRT / .SLDASM files.
    explicit Repository(const fs::path& project_dir);

    // Destructor defined in .cpp (SQLite::Database must be complete there).
    ~Repository();

    // Returns true if the repo was opened / created successfully.
    bool IsValid() const { return valid_; }

    // -------------------------------------------------------
    // Commits
    // -------------------------------------------------------

    // Persist a new commit record to the database.
    Result SaveCommit(const Commit& c);

    // Load a commit by its full hash or a 7+ char prefix.
    Result LoadCommit(const std::string& hash_prefix, Commit& out);

    // Return all commits, newest first.
    std::vector<Commit> ListCommits();

    // -------------------------------------------------------
    // HEAD management
    // -------------------------------------------------------
    std::string GetHead();
    Result      SetHead(const std::string& hash);

    // -------------------------------------------------------
    // File paths — blobs and thumbnails stay on disk
    // -------------------------------------------------------
    fs::path BlobPath(const std::string& hash) const;
    fs::path ThumbnailPath(const std::string& hash) const;

    // -------------------------------------------------------
    // Directory paths
    // -------------------------------------------------------
    fs::path Root()     const { return repo_root_; }
    fs::path BlobsDir() const { return repo_root_ / "blobs"; }

private:
    fs::path project_dir_;
    fs::path repo_root_;   // project_dir_ / ".swvcs"
    bool     valid_ = false;

    std::unique_ptr<SQLite::Database> db_;

    void Init();        // create dirs, open DB
    void InitSchema();  // CREATE TABLE IF NOT EXISTS
};
