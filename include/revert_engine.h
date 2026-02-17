#pragma once

// -------------------------------------------------------
// RevertEngine
// -------------------------------------------------------
// Restores the working file to a previously committed state:
//   1. Close the document in SolidWorks (so the file is unlocked)
//   2. Copy the blob back over the working file
//   3. Reopen the file in SolidWorks
//   4. Update HEAD
// -------------------------------------------------------

#include "types.h"
#include "repository.h"
#include "sw_connection.h"

class RevertEngine {
public:
    RevertEngine(Repository& repo, SwConnection& sw);

    // Revert the working file to the given commit (hash prefix OK).
    // The original file will be overwritten — make sure you have committed
    // or are intentionally discarding unsaved changes.
    Result Revert(const std::string& hash_prefix);

private:
    Repository&   repo_;
    SwConnection& sw_;
};