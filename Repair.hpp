#ifndef ARGXX_REPAIR_HPP
#define ARGXX_REPAIR_HPP
#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>
namespace argxx {
using Json = nlohmann::json;
using Responder = std::function<std::string(const Json&, const Json&, double)>;
Json defaultRepairConfig();
std::string repairPrompt();
std::string readText(const std::filesystem::path& path, size_t limit = 2000000);
void writeText(const std::filesystem::path& path, const std::string& value);
void writeJson(const std::filesystem::path& path, const Json& value);
std::filesystem::path safeRepoPath(const std::filesystem::path& root, const std::string& name);
void validateTask(const Json& task);
void validateConfig(const Json& config);
std::string ollamaAction(const Json& messages, const Json& config, double timeout);
Json repair(const Json& task, const Json& config, const std::filesystem::path& output,
            const Responder& responder = ollamaAction);
Json evaluate(const Json& tasks, const Json& config, const std::filesystem::path& artifacts,
              const Responder& responder = ollamaAction);
void initialize(const std::filesystem::path& artifacts);
std::filesystem::path makeFixtures(const std::filesystem::path& artifacts);
void exportTraining(const std::filesystem::path& run, const std::filesystem::path& artifacts);
int runCli(int argc, char** argv);
}  // namespace argxx
#endif
