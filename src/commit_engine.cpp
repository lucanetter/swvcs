#include "commit_engine.h"
#include "repository.h"
#include "sw_connection.h"

#include <Windows.h>
#include <wincrypt.h>   // CryptAcquireContext, SHA-256
#pragma comment(lib, "advapi32.lib")

#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

CommitEngine::CommitEngine(Repository& repo, SwConnection& sw)
    : repo_(repo), sw_(sw) {}

// -------------------------------------------------------
// Commit
// -------------------------------------------------------

Result CommitEngine::Commit(const std::string& message, bool capture_thumbnail) {
    // 1. Get info about the active document
    ActiveDocInfo doc_info;
    Result r = sw_.GetActiveDocInfo(doc_info);
    if (!r.ok) return r;

    if (doc_info.path.empty())
        return Result::failure("Active document has not been saved yet (no file path).");

    // 2. Save the document so the file on disk is up-to-date
    r = sw_.SaveActiveDoc();
    if (!r.ok) {
        std::cerr << "[commit] Warning: save failed (" << r.err << "), continuing with file as-is.\n";
    }

    fs::path src_path(doc_info.path);
    if (!fs::exists(src_path))
        return Result::failure("File not found on disk: " + doc_info.path);

    // 3. Compute hash of the file
    std::string hash = HashFile(src_path);
    if (hash.empty())
        return Result::failure("Failed to hash file: " + doc_info.path);

    // 4. Copy blob into repo
    fs::path blob_dest = repo_.BlobPath(hash);
    if (fs::exists(blob_dest)) {
        std::cout << "[commit] Identical snapshot already stored (hash: " << hash.substr(0,8) << "...)\n";
        // Still create a new commit record pointing to this blob
    } else {
        r = CopyBlob(src_path, blob_dest);
        if (!r.ok) return r;
        std::cout << "[commit] Stored blob " << blob_dest.filename().string() << "\n";
    }

    // 5. Thumbnail (best-effort — don't fail the commit if this fails)
    if (capture_thumbnail) {
        fs::path thumb_dest = repo_.ThumbnailPath(hash);
        Result tr = sw_.SaveThumbnail(thumb_dest.string());
        if (!tr.ok) {
            std::cerr << "[commit] Thumbnail skipped: " << tr.err << "\n";
        }
    }

    // 6. Gather SW metadata
    ::Commit c;
    c.hash        = hash;
    c.message     = message;
    c.timestamp   = NowISO8601();
    c.parent_hash = repo_.GetHead();
    c.author      = GetAuthor();

    c.sw_meta.doc_path = doc_info.path;
    c.sw_meta.doc_type = doc_info.type;

    double mass = 0, volume = 0, surface_area = 0;
    sw_.GetMassProperties(mass, volume, surface_area);
    c.sw_meta.mass         = mass;
    c.sw_meta.volume       = volume;
    c.sw_meta.surface_area = surface_area;

    int feat_count = 0;
    sw_.GetFeatureCount(feat_count);
    c.sw_meta.feature_count = feat_count;

    sw_.GetMaterial(c.sw_meta.material);
    sw_.GetBoundingBox(c.sw_meta.bbox_x, c.sw_meta.bbox_y, c.sw_meta.bbox_z);
    sw_.GetConfigCount(c.sw_meta.config_count);

    // Blob file size (filesystem — no COM needed)
    std::error_code size_ec;
    c.sw_meta.blob_size_bytes =
        static_cast<int64_t>(fs::file_size(blob_dest, size_ec));
    if (size_ec) c.sw_meta.blob_size_bytes = 0;

    // 7. Persist commit record and update HEAD
    r = repo_.SaveCommit(c);
    if (!r.ok) return r;

    r = repo_.SetHead(hash);
    if (!r.ok) return r;

    std::cout << "[commit] Created commit " << hash.substr(0,8) << " \""  << message << "\"\n";
    return Result::success();
}

// -------------------------------------------------------
// HashFile  (SHA-256 via Windows CryptoAPI)
// -------------------------------------------------------

std::string CommitEngine::HashFile(const fs::path& path) {
    HCRYPTPROV prov   = 0;
    HCRYPTHASH hash_h = 0;
    std::string result;

    if (!CryptAcquireContext(&prov, nullptr, nullptr,
                             PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return "";

    if (!CryptCreateHash(prov, CALG_SHA_256, 0, 0, &hash_h)) {
        CryptReleaseContext(prov, 0);
        return "";
    }

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        CryptDestroyHash(hash_h);
        CryptReleaseContext(prov, 0);
        return "";
    }

    const size_t CHUNK = 65536;
    std::vector<char> buf(CHUNK);
    while (f) {
        f.read(buf.data(), CHUNK);
        std::streamsize n = f.gcount();
        if (n > 0) {
            CryptHashData(hash_h,
                          reinterpret_cast<BYTE*>(buf.data()),
                          static_cast<DWORD>(n), 0);
        }
    }

    DWORD hash_len = 32;
    BYTE  hash_bytes[32];
    if (CryptGetHashParam(hash_h, HP_HASHVAL, hash_bytes, &hash_len, 0)) {
        std::ostringstream oss;
        for (int i = 0; i < 32; ++i)
            oss << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(hash_bytes[i]);
        result = oss.str();
    }

    CryptDestroyHash(hash_h);
    CryptReleaseContext(prov, 0);
    return result;
}

// -------------------------------------------------------
// CopyBlob
// -------------------------------------------------------

Result CommitEngine::CopyBlob(const fs::path& src, const fs::path& dst) {
    std::error_code ec;
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec) return Result::failure("Failed to copy blob: " + ec.message());
    return Result::success();
}

// -------------------------------------------------------
// NowISO8601
// -------------------------------------------------------

std::string CommitEngine::NowISO8601() {
    auto now   = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    gmtime_s(&tm_buf, &t);   // UTC
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return buf;
}

// -------------------------------------------------------
// GetAuthor  (machine username)
// -------------------------------------------------------

std::string CommitEngine::GetAuthor() {
    char buf[256] = {};
    DWORD len = sizeof(buf);
    GetUserNameA(buf, &len);
    return buf;
}
