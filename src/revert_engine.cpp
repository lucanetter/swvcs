#include "revert_engine.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

RevertEngine::RevertEngine(Repository& repo, SwConnection& sw)
    : repo_(repo), sw_(sw) {}

Result RevertEngine::Revert(const std::string& hash_prefix) {
    // 1. Resolve the commit
    Commit target;
    Result r = repo_.LoadCommit(hash_prefix, target);
    if (!r.ok) return r;

    fs::path blob_path = repo_.BlobPath(target.hash);
    if (!fs::exists(blob_path))
        return Result::failure("Blob missing for commit " + target.hash.substr(0,8)
                               + " — was the repo moved?");

    fs::path doc_path(target.sw_meta.doc_path);

    std::cout << "[revert] Reverting to commit " << target.hash.substr(0,8)
              << " \"" << target.message << "\"\n";

    // 2. Close the document in SolidWorks (releases the file lock)
    bool doc_was_open = false;
    if (sw_.IsConnected()) {
        ActiveDocInfo info;
        if (sw_.GetActiveDocInfo(info).ok && !info.path.empty()) {
            doc_was_open = true;
            std::cout << "[revert] Closing document in SolidWorks...\n";
            Result cr = sw_.CloseActiveDoc(/*force_close=*/true);
            if (!cr.ok) {
                std::cerr << "[revert] Warning: could not close document: " << cr.err
                          << "\n         Attempting to overwrite anyway.\n";
            }
        }
    }

    // 3. Overwrite working file with blob
    std::error_code ec;
    fs::copy_file(blob_path, doc_path, fs::copy_options::overwrite_existing, ec);
    if (ec)
        return Result::failure("Failed to restore file: " + ec.message());

    std::cout << "[revert] Restored: " << doc_path.string() << "\n";

    // 4. Reopen in SolidWorks
    if (sw_.IsConnected() && doc_was_open) {
        std::cout << "[revert] Reopening in SolidWorks...\n";
        Result or_ = sw_.OpenDoc(doc_path.string());
        if (!or_.ok)
            std::cerr << "[revert] Warning: could not reopen file: " << or_.err << "\n";
    }

    // 5. Update HEAD
    r = repo_.SetHead(target.hash);
    if (!r.ok) return r;

    std::cout << "[revert] Done. HEAD is now " << target.hash.substr(0,8) << "\n";
    return Result::success();
}
