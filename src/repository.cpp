#include "repository.h"

#include <SQLiteCpp/SQLiteCpp.h>

#include <filesystem>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

// -------------------------------------------------------
// Construction / destruction
// -------------------------------------------------------

Repository::Repository(const fs::path& project_dir)
    : project_dir_(project_dir)
    , repo_root_(project_dir / ".swvcs")
{
    Init();
}

// Must be defined here (not in the header) because
// SQLite::Database is only a complete type in this TU.
Repository::~Repository() = default;

// -------------------------------------------------------
// Init — create folder structure and open the database
// -------------------------------------------------------

void Repository::Init()
{
    std::error_code ec;
    fs::create_directories(repo_root_,              ec);
    fs::create_directories(BlobsDir(),              ec);
    fs::create_directories(repo_root_ / "thumbs",  ec);

    if (ec) {
        std::cerr << "[repo] Failed to create repo dirs: " << ec.message() << "\n";
        return;
    }

    try {
        fs::path db_path = repo_root_ / "swvcs.db";
        db_ = std::make_unique<SQLite::Database>(
            db_path.string(),
            SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

        InitSchema();
        valid_ = true;
        std::cout << "[repo] Repository at: " << repo_root_.string() << "\n";
    }
    catch (const SQLite::Exception& e) {
        std::cerr << "[repo] Database error: " << e.what() << "\n";
    }
}

void Repository::InitSchema()
{
    // commits table — one row per snapshot
    db_->exec(R"(
        CREATE TABLE IF NOT EXISTS commits (
            hash          TEXT PRIMARY KEY,
            message       TEXT NOT NULL DEFAULT '',
            timestamp     TEXT NOT NULL DEFAULT '',
            author        TEXT NOT NULL DEFAULT '',
            parent_hash   TEXT NOT NULL DEFAULT '',
            doc_path      TEXT NOT NULL DEFAULT '',
            doc_type      TEXT NOT NULL DEFAULT '',
            mass          REAL NOT NULL DEFAULT 0.0,
            volume        REAL NOT NULL DEFAULT 0.0,
            feature_count INTEGER NOT NULL DEFAULT 0
        );
    )");

    // config table — key/value store for HEAD, version, etc.
    db_->exec(R"(
        CREATE TABLE IF NOT EXISTS config (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL DEFAULT ''
        );
    )");

    // Seed version on first creation (ignored if already exists)
    db_->exec("INSERT OR IGNORE INTO config (key, value) VALUES ('version', '2');");
    db_->exec("INSERT OR IGNORE INTO config (key, value) VALUES ('HEAD', '');");
}

// -------------------------------------------------------
// HEAD
// -------------------------------------------------------

std::string Repository::GetHead()
{
    if (!valid_) return "";
    try {
        SQLite::Statement q(*db_, "SELECT value FROM config WHERE key = 'HEAD'");
        if (q.executeStep())
            return q.getColumn(0).getString();
    }
    catch (const SQLite::Exception& e) {
        std::cerr << "[repo] GetHead error: " << e.what() << "\n";
    }
    return "";
}

Result Repository::SetHead(const std::string& hash)
{
    if (!valid_) return Result::failure("Repository not valid");
    try {
        SQLite::Statement q(*db_,
            "INSERT OR REPLACE INTO config (key, value) VALUES ('HEAD', ?)");
        q.bind(1, hash);
        q.exec();
        return Result::success();
    }
    catch (const SQLite::Exception& e) {
        return Result::failure(std::string("SetHead DB error: ") + e.what());
    }
}

// -------------------------------------------------------
// Blob / thumbnail paths  (files stay on disk)
// -------------------------------------------------------

fs::path Repository::BlobPath(const std::string& hash) const {
    return BlobsDir() / (hash + ".bin");
}

fs::path Repository::ThumbnailPath(const std::string& hash) const {
    return repo_root_ / "thumbs" / (hash + ".bmp");
}

// -------------------------------------------------------
// SaveCommit
// -------------------------------------------------------

Result Repository::SaveCommit(const Commit& c)
{
    if (!valid_) return Result::failure("Repository not valid");
    if (c.hash.empty()) return Result::failure("Commit has no hash");

    try {
        SQLite::Statement q(*db_, R"(
            INSERT OR REPLACE INTO commits
                (hash, message, timestamp, author, parent_hash,
                 doc_path, doc_type, mass, volume, feature_count)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )");
        q.bind(1,  c.hash);
        q.bind(2,  c.message);
        q.bind(3,  c.timestamp);
        q.bind(4,  c.author);
        q.bind(5,  c.parent_hash);
        q.bind(6,  c.sw_meta.doc_path);
        q.bind(7,  c.sw_meta.doc_type);
        q.bind(8,  c.sw_meta.mass);
        q.bind(9,  c.sw_meta.volume);
        q.bind(10, c.sw_meta.feature_count);
        q.exec();
        return Result::success();
    }
    catch (const SQLite::Exception& e) {
        return Result::failure(std::string("SaveCommit DB error: ") + e.what());
    }
}

// -------------------------------------------------------
// LoadCommit  (exact hash or 7+ char prefix)
// -------------------------------------------------------

static Commit RowToCommit(SQLite::Statement& q)
{
    Commit c;
    c.hash              = q.getColumn(0).getString();
    c.message           = q.getColumn(1).getString();
    c.timestamp         = q.getColumn(2).getString();
    c.author            = q.getColumn(3).getString();
    c.parent_hash       = q.getColumn(4).getString();
    c.sw_meta.doc_path  = q.getColumn(5).getString();
    c.sw_meta.doc_type  = q.getColumn(6).getString();
    c.sw_meta.mass      = q.getColumn(7).getDouble();
    c.sw_meta.volume    = q.getColumn(8).getDouble();
    c.sw_meta.feature_count = q.getColumn(9).getInt();
    return c;
}

Result Repository::LoadCommit(const std::string& hash_prefix, Commit& out)
{
    if (!valid_) return Result::failure("Repository not valid");
    try {
        // Try exact match first
        {
            SQLite::Statement q(*db_,
                "SELECT * FROM commits WHERE hash = ? LIMIT 1");
            q.bind(1, hash_prefix);
            if (q.executeStep()) {
                out = RowToCommit(q);
                return Result::success();
            }
        }
        // Fall back to prefix match
        {
            SQLite::Statement q(*db_,
                "SELECT * FROM commits WHERE hash LIKE ? LIMIT 1");
            q.bind(1, hash_prefix + "%");
            if (q.executeStep()) {
                out = RowToCommit(q);
                return Result::success();
            }
        }
        return Result::failure("No commit found matching: " + hash_prefix);
    }
    catch (const SQLite::Exception& e) {
        return Result::failure(std::string("LoadCommit DB error: ") + e.what());
    }
}

// -------------------------------------------------------
// ListCommits  (newest first)
// -------------------------------------------------------

std::vector<Commit> Repository::ListCommits()
{
    std::vector<Commit> commits;
    if (!valid_) return commits;
    try {
        SQLite::Statement q(*db_,
            "SELECT * FROM commits ORDER BY timestamp DESC");
        while (q.executeStep())
            commits.push_back(RowToCommit(q));
    }
    catch (const SQLite::Exception& e) {
        std::cerr << "[repo] ListCommits error: " << e.what() << "\n";
    }
    return commits;
}
