#include <unistd.h>

#include <chrono>
#include <iostream>

#include "Process.hpp"
#include "Repair.hpp"
using argxx::Json;
namespace fs = std::filesystem;
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
argxx::Responder scripted(const Json& actions) {
    return [actions, index = size_t{0}](const Json&, const Json&, double) mutable {
        if (index >= actions.size()) throw std::runtime_error("Script exhausted");
        return actions.at(index++).dump();
    };
}
int main() {
    const auto root = fs::temp_directory_path() /
                      ("argxx-native-" + std::to_string(getpid()) + "-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fs::create_directories(root);
        const auto tasks = Json::parse(argxx::readText(argxx::makeFixtures(root)));
        const auto task = tasks.at(0);
        const auto source = fs::path(task.at("repo").get<std::string>()) / "calculator.hpp";
        const auto original = argxx::readText(source);
        auto config = argxx::defaultRepairConfig();
        const Json fix = {{"action", "edit"},
                          {"path", "calculator.hpp"},
                          {"old", "return sum / values.size();"},
                          {"new", "return values.empty() ? 0 : sum / values.size();"}};
        const Json finish = {{"action", "finish"}};
        auto result = argxx::repair(
            task, config, root / "success",
            scripted(Json::array(
                {{{"action", "read"}, {"path", "calculator.hpp"}},
                 {{"action", "edit"},
                  {"path", "calculator.hpp"},
                  {"old", "return sum / values.size();"},
                  {"new", "return 1;"}},
                 {{"action", "test"}},
                 {{"action", "edit"},
                  {"path", "calculator.hpp"},
                  {"old", "return 1;"},
                  {"new", "return values.empty() ? 0 : sum / values.size();"}},
                 {{"action", "write"}, {"path", "helper.hpp"}, {"content", "#pragma once\n"}},
                 finish})));
        require(result.at("status") == "passed", "Revision/replay failed: " + result.dump());
        require(!result.at("baseline_passed").get<bool>(), "Baseline should fail");
        require(Json::parse(argxx::readText(root / "success/test-3.json")).at("returncode") != 0,
                "Intermediate edit should fail");
        require(argxx::readText(source) == original, "Original changed");
        require(fs::exists(root / "success/validation/helper.hpp"), "New file missing from replay");
        require(argxx::readText(root / "success/patch.diff").find("diff --git") == 0,
                "Patch missing");
        config["max_steps"] = 4;
        result = argxx::repair(task, config, root / "malformed-action",
                               scripted(Json::array({{{"action", 3}}, fix, finish})));
        require(result.at("status") == "passed", "Malformed action should allow retry");
        result = argxx::repair(
            task, config, root / "protection",
            scripted(
                Json::array({{{"action", "write"}, {"path", "../escape"}, {"content", "bad"}},
                             {{"action", "write"}, {"path", ".git/config"}, {"content", "bad"}},
                             {{"action", "edit"},
                              {"path", "test_calculator.cpp"},
                              {"old", "return"},
                              {"new", "bad"}},
                             {{"action", "edit"},
                              {"path", "calculator.hpp"},
                              {"old", "missing"},
                              {"new", "bad"}}})));
        require(result.at("status") == "failed", "Invalid actions must not pass");
        for (const auto& turn : Json::parse(argxx::readText(root / "protection/trace.json")))
            require(turn.at("result").contains("error"), "Action should be rejected");
        fs::create_directory_symlink(root, root / "link");
        bool rejected = false;
        try {
            argxx::safeRepoPath(root, "link/outside");
        } catch (const std::exception&) {
            rejected = true;
        }
        require(rejected, "Symlink accepted");
        auto validationTask = task;
        validationTask["validation_command"] = {"sh", "-c", "exit 1"};
        result = argxx::repair(validationTask, config, root / "validation-fails",
                               scripted(Json::array({fix, finish})));
        require(result.at("status") == "failed", "Independent validation failure ignored");
        result =
            argxx::repair(task, config, root / "inference-fails", scripted(Json::array({fix})));
        require(result.at("status") == "error" &&
                    fs::file_size(root / "inference-fails/patch.diff") > 0,
                "Partial patch lost");
        auto timeout = argxx::runProcess({"sh", "-c", "sleep 10"}, root, .05);
        require(timeout.timedOut, "Timeout missing");
        const auto noisy = argxx::runProcess({"sh", "-c", "yes x | head -c 100000"}, root, 5, 1024);
        require(noisy.truncated && noisy.output.size() == 1024, "Subprocess output not bounded");
        const auto literal =
            argxx::runProcess({"printf", "%s", "$(touch injected); spaced argument"}, root, 2);
        require(literal.output == "$(touch injected); spaced argument" &&
                    !fs::exists(root / "injected"),
                "Command argument injection");
        argxx::initialize(root);
        argxx::writeText(root / "prompts/system.md", "custom prompt");
        argxx::initialize(root);
        require(argxx::readText(root / "prompts/system.md") == "custom prompt",
                "Init overwrote config");
        config["max_steps"] = 0;
        rejected = false;
        try {
            argxx::validateConfig(config);
        } catch (const std::exception&) {
            rejected = true;
        }
        require(rejected, "Invalid budget accepted");
        fs::remove_all(root);
        std::cout << "Native repair regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nPreserved test artifacts: " << root << '\n';
        return 1;
    }
}
