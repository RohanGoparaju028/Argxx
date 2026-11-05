#ifndef ARGXX_PROCESS_HPP
#define ARGXX_PROCESS_HPP
#include <filesystem>
#include <string>
#include <vector>
namespace argxx {
struct ProcessResult {
    int code = -1;
    bool timedOut = false;
    bool truncated = false;
    double seconds = 0;
    // cppcheck-suppress unusedStructMember
    std::string output;
};
ProcessResult runProcess(const std::vector<std::string>& args, const std::filesystem::path& cwd,
                         double timeout, size_t outputLimit = 16000);
std::string checkedProcess(const std::vector<std::string>& args, const std::filesystem::path& cwd,
                           double timeout = 30, size_t outputLimit = 2000000);
}  // namespace argxx
#endif
