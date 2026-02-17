#include "repository.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;
namespace fs = std::filesystem;

// -------------------------------------------------------
// Construction / init
// -------------------------------------------------------

Repository::Repository(const fs::path& project_dir)
    : project_dir_(project_dir)
    , repo_root_(project_dir / ".swvcs")
{
    Init();
}

void Repository::Init() {
    std::error_code ec;

    // Create directory structure if it doesn't exist
    fs::create_directories(repo_root_,          ec);
    fs::create_directories(CommitsDir(),        ec);
    fs::create_directories(BlobsDir(),          ec);
    fs::create_directories(repo_root_ / "thumbs", ec);

    if (ec) {
        std::cerr << "[repo] Failed to create repo dirs: " << ec.message() << "\n";
        return;
    }

    // Create HEAD file if missing
    fs::path head_path = repo_root_ / "HEAD";
    if (!fs::exists(head_path)) {
        std::ofstream(head_path) << "";  // empty = no commits yet
    }

    // Create config if missing
    fs::path cfg = repo_root_ / "config.json";
    if (!fs::exists(cfg)) {
        json config;
        config["version"] = 1;
        config["created"] = "unknown";
        std::ofstream(cfg) << config.dump(2);
    }

    valid_ = true;
    std::cout << "[repo] Repository at: " << repo_root_.string() << "\n";
}

// -------------------------------------------------------
// HEAD
// -------------------------------------------------------

std::string Repository::GetHead() const {
    fs::path head_path = repo_root_ / "HEAD";
    std::ifstream f(head_path);
    std::string hash;
    std::getline(f, hash);
    return hash;
}

Result Repository::SetHead(const std::string& hash) {
    fs::path head_path = repo_root_ / "HEAD";
    std::ofstream f(head_path);
    if (!f) return Result::failure("Could not write HEAD");
    f << hash;
    return Result::success();
}

// -------------------------------------------------------
// Blob / thumbnail paths
// -------------------------------------------------------

fs::path Repository::BlobPath(const std::string& hash) const {
    return BlobsDir() / (hash + ".bin");
}

fs::path Repository::ThumbnailPath(const std::string& hash) const {
    return repo_root_ / "thumbs" / (hash + ".bmp");
}

// -------------------------------------------------------
// JSON serialisation helpers
// -------------------------------------------------------

static json CommitToJson(const Commit& c) {
    json j;
    j["hash"]        = c.hash;
    j["message"]     = c.message;
    j["timestamp"]   = c.timestamp;
    j["parent_hash"] = c.parent_hash;
    j["author"]      = c.author;
    j["sw"]["doc_path"]     = c.sw_meta.doc_path;
    j["sw"]["doc_type"]     = c.sw_meta.doc_type;
    j["sw"]["mass"]         = c.sw_meta.mass;
    j["sw"]["volume"]       = c.sw_meta.volume;
    j["sw"]["feature_count"]= c.sw_meta.feature_count;
    return j;
}

static Commit CommitFromJson(const json& j) {
    Commit c;
    c.hash        = j.value("hash",        "");
    c.message     = j.value("message",     "");
    c.timestamp   = j.value("timestamp",   "");
    c.parent_hash = j.value("parent_hash", "");
    c.author      = j.value("author",      "");

    if (j.contains("sw")) {
        auto& sw            = j["sw"];
        c.sw_meta.doc_path     = sw.value("doc_path",      "");
        c.sw_meta.doc_type     = sw.value("doc_type",      "");
        c.sw_meta.mass         = sw.value("mass",          0.0);
        c.sw_meta.volume       = sw.value("volume",        0.0);
        c.sw_meta.feature_count= sw.value("feature_count", 0);
    }
    return c;
}

// -------------------------------------------------------
// SaveCommit
// -------------------------------------------------------

Result Repository::SaveCommit(const Commit& c) {
    if (!valid_) return Result::failure("Repository not valid");
    if (c.hash.empty()) return Result::failure("Commit has no hash");

    fs::path commit_file = CommitsDir() / (c.hash + ".json");
    std::ofstream f(commit_file);
    if (!f) return Result::failure("Could not write commit file: " + commit_file.string());

    f << CommitToJson(c).dump(2);
    return Result::success();
}

// -------------------------------------------------------
// LoadCommit  (supports prefix lookup)
// -------------------------------------------------------

Result Repository::LoadCommit(const std::string& hash_prefix, Commit& out) {
    if (!valid_) return Result::failure("Repository not valid");

    for (auto& entry : fs::directory_iterator(CommitsDir())) {
        std::string filename = entry.path().stem().string();
        if (filename.find(hash_prefix) == 0) {
            std::ifstream f(entry.path());
            if (!f) return Result::failure("Could not read commit file");
            json j = json::parse(f);
            out = CommitFromJson(j);
            return Result::success();
        }
    }
    return Result::failure("No commit found matching: " + hash_prefix);
}

// -------------------------------------------------------
// ListCommits  (newest first by timestamp string)
// -------------------------------------------------------

std::vector<Commit> Repository::ListCommits() {
    std::vector<Commit> commits;
    if (!valid_) return commits;

    for (auto& entry : fs::directory_iterator(CommitsDir())) {
        if (entry.path().extension() != ".json") continue;
        std::ifstream f(entry.path());
        if (!f) continue;
        try {
            json j = json::parse(f);
            commits.push_back(CommitFromJson(j));
        } catch (...) {
            // malformed commit file — skip
        }
    }

    // Sort newest first (ISO-8601 timestamps sort lexicographically)
    std::sort(commits.begin(), commits.end(), [](const Commit& a, const Commit& b){
        return a.timestamp > b.timestamp;
    });

    return commits;
}