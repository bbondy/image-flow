#include "cli_impl.h"

#include "cli_args.h"
#include "cli_help.h"
#include "cli_ops.h"
#include "cli_project_cmds.h"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {
int runCommand(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: image_flow <new|info|render|ops|help> ...\n";
        return 1;
    }

    const std::string sub = args[1];
    if (sub == "new") {
        return runIFLOWNew(args);
    }
    if (sub == "info") {
        return runIFLOWInfo(args);
    }
    if (sub == "render") {
        return runIFLOWRender(args);
    }
    if (sub == "ops") {
        return runIFLOWOps(args);
    }

    std::cerr << "Unknown command: " << sub << "\n";
    return 1;
}
} // namespace

int runCLIImpl(int argc, char** argv) {
    try {
        const std::vector<std::string> args = collectArgs(argc, argv);
        if (args.size() <= 1) {
            writeUsage();
            return 1;
        }

        const std::string command = args[1];
        if (command == "help" || command == "--help" || command == "-h") {
            if (args.size() >= 3 && args[2] == "ops") {
                writeOpsUsage();
                return 0;
            }
            writeUsage();
            return 0;
        }
        return runCommand(args);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
