#include "cli_args.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

std::vector<std::string> collectArgs(int argc, char** argv) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return args;
}

bool getFlagValue(const std::vector<std::string>& args, const std::string& flag, std::string& value) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) {
            value = args[i + 1];
            return true;
        }
    }
    return false;
}

std::vector<std::string> getFlagValues(const std::vector<std::string>& args, const std::string& flag) {
    std::vector<std::string> values;
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) {
            values.push_back(args[i + 1]);
        }
    }
    return values;
}

void addOpsFromStream(std::istream& in, std::vector<std::string>& outOps) {
    std::string line;
    while (std::getline(in, line)) {
        std::size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            continue;
        }
        if (line[start] == '#') {
            continue;
        }
        const std::size_t end = line.find_last_not_of(" \t\r\n");
        outOps.push_back(line.substr(start, end - start + 1));
    }
}

std::vector<std::string> gatherOps(const std::vector<std::string>& args) {
    std::vector<std::string> ops = getFlagValues(args, "--op");

    std::string opsFilePath;
    if (getFlagValue(args, "--ops-file", opsFilePath)) {
        std::ifstream file(opsFilePath);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open ops file: " + opsFilePath);
        }
        addOpsFromStream(file, ops);
    }

    const bool useStdin = std::find(args.begin(), args.end(), "--stdin") != args.end();
    if (useStdin) {
        addOpsFromStream(std::cin, ops);
    }

    return ops;
}
