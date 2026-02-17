#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

#include "sw_connection.h"
#include "repository.h"
#include "commit_engine.h"
#include "revert_engine.h"
#include "utils.h"

namespace fs = std::filesystem;

// -------------------------------------------------------
// Print usage
// -------------------------------------------------------
static void PrintHelp() {
    std::cout <<
R"(swvcs - SolidWorks Version Control System

Usage:
  swvcs <command> [options]

Commands:
  init    [dir]          Initialise a repository in [dir] (default: current dir)
  status                 Show HEAD commit and active document info
  commit  <message>      Snapshot the active SolidWorks document
  log     [--full]       List all commits (newest first)
  revert  <hash>         Restore working file to a previous commit

Examples:
  swvcs init C:\Projects\BracketDesign
  swvcs commit "Added fillet to top edge"
  swvcs log
  swvcs revert a1b2c3d4

Notes:
  - SolidWorks must be running for commit and revert.
  - A hash prefix of 7+ characters is sufficient for revert.
)";
}

// -------------------------------------------------------
// Commands
// -------------------------------------------------------

static int CmdInit(const std::vector<std::string>& args) {
    fs::path dir = args.size() > 1 ? fs::path(args[1]) : fs::current_path();
    Repository repo(dir);
    if (!repo.IsValid()) {
        std::cerr << "Failed to initialise repository at: " << dir.string() << "\n";
        return 1;
    }
    std::cout << "Initialised swvcs repository at: " << repo.Root().string() << "\n";
    return 0;
}

static int CmdStatus(const std::vector<std::string>& args, Repository& repo, SwConnection& sw) {
    std::string head = repo.GetHead();
    if (head.empty()) {
        std::cout << "No commits yet.\n";
    } else {
        Commit c;
        if (repo.LoadCommit(head, c).ok) {
            std::cout << "HEAD: " << head.substr(0,8) << " \"" << c.message << "\"\n"
                      << "Date: " << c.timestamp << "\n\n";
        }
    }

    if (!sw.IsConnected()) {
        std::cout << "SolidWorks: not connected\n";
        return 0;
    }

    ActiveDocInfo info;
    if (!sw.GetActiveDocInfo(info).ok) {
        std::cout << "SolidWorks: connected, no active document\n";
        return 0;
    }

    std::cout << "Active document:\n"
              << "  Path:  " << info.path  << "\n"
              << "  Type:  " << info.type  << "\n"
              << "  Dirty: " << (info.is_dirty ? "yes (unsaved changes)" : "no") << "\n";
    return 0;
}

static int CmdCommit(const std::vector<std::string>& args,
                     Repository& repo, SwConnection& sw) {
    if (args.size() < 2) {
        std::cerr << "Usage: swvcs commit <message>\n";
        return 1;
    }

    if (!sw.IsConnected()) {
        std::cerr << "SolidWorks is not running.\n";
        return 1;
    }

    // Join all remaining args as the message (handles spaces without quotes)
    std::string message;
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) message += " ";
        message += args[i];
    }

    CommitEngine engine(repo, sw);
    Result r = engine.Commit(message);
    if (!r.ok) {
        std::cerr << "Commit failed: " << r.err << "\n";
        return 1;
    }
    return 0;
}

static int CmdLog(const std::vector<std::string>& args, Repository& repo) {
    bool full = args.size() > 1 && args[1] == "--full";
    auto commits = repo.ListCommits();

    if (commits.empty()) {
        std::cout << "No commits yet.\n";
        return 0;
    }

    std::string head = repo.GetHead();
    for (const auto& c : commits) {
        if (c.hash == head) std::cout << "* ";
        else                std::cout << "  ";
        Utils::PrintCommit(c, full);
    }
    return 0;
}

static int CmdRevert(const std::vector<std::string>& args,
                     Repository& repo, SwConnection& sw) {
    if (args.size() < 2) {
        std::cerr << "Usage: swvcs revert <hash>\n";
        return 1;
    }

    // Confirm with user
    std::cout << "This will overwrite your working file with commit "
              << args[1] << ".\nContinue? [y/N] ";
    std::string ans;
    std::getline(std::cin, ans);
    if (ans.empty() || (ans[0] != 'y' && ans[0] != 'Y')) {
        std::cout << "Aborted.\n";
        return 0;
    }

    RevertEngine engine(repo, sw);
    Result r = engine.Revert(args[1]);
    if (!r.ok) {
        std::cerr << "Revert failed: " << r.err << "\n";
        return 1;
    }
    return 0;
}

// -------------------------------------------------------
// main
// -------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintHelp();
        return 0;
    }

    std::string cmd = argv[1];
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) args.push_back(argv[i]);

    // init doesn't need a repo or SW connection
    if (cmd == "init") {
        return CmdInit(args);
    }

    if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        PrintHelp();
        return 0;
    }

    // All other commands need a repo in the current directory
    Repository repo(fs::current_path());
    if (!repo.IsValid()) {
        std::cerr << "No swvcs repository found in: " << fs::current_path().string()
                  << "\nRun 'swvcs init' first.\n";
        return 1;
    }

    // Try to connect to SolidWorks (non-fatal — log/status can work offline)
    SwConnection sw;
    SwConnectStatus sw_status = sw.Connect();
    if (sw_status != SwConnectStatus::OK) {
        if (cmd == "commit" || cmd == "revert") {
            std::cerr << "SolidWorks must be running for '" << cmd << "'.\n";
            return 1;
        }
        std::cout << "[swvcs] Note: SolidWorks not running — some features unavailable.\n\n";
    }

    // Dispatch command
    if      (cmd == "status") return CmdStatus(args, repo, sw);
    else if (cmd == "commit") return CmdCommit(args, repo, sw);
    else if (cmd == "log")    return CmdLog(args, repo);
    else if (cmd == "revert") return CmdRevert(args, repo, sw);
    else {
        std::cerr << "Unknown command: " << cmd << "\n";
        PrintHelp();
        return 1;
    }
}