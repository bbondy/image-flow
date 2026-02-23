#ifndef CLI_ARGS_H
#define CLI_ARGS_H

#include <istream>
#include <string>
#include <vector>

std::vector<std::string> collectArgs(int argc, char** argv);
bool getFlagValue(const std::vector<std::string>& args, const std::string& flag, std::string& value);
std::vector<std::string> getFlagValues(const std::vector<std::string>& args, const std::string& flag);
void addOpsFromStream(std::istream& in, std::vector<std::string>& outOps);
std::vector<std::string> gatherOps(const std::vector<std::string>& args);

#endif
