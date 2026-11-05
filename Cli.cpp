#include <cpr/cpr.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <set>

#include "Process.hpp"
#include "Repair.hpp"

namespace argxx {
namespace fs = std::filesystem;
namespace {
const char* help = R"(Argxx 0.2.0 — local repository repair assistant (macOS/Linux)

Usage:
  agent init [--artifacts DIR]
  agent doctor [--config FILE] [--model NAME]
  agent repair --repo DIR --issue TEXT --test-command '["program","arg"]'
  agent repair --task task.json
  agent evaluate --manifest tasks.json
  agent fixtures
  agent export-training --run DIR
  agent package
  agent chat [owner/repository]       GitHub analysis/chat (requires agent.yaml)

Repair/evaluation options:
  --artifacts DIR       Output root (default: nearest repo's .Argxx/.artifacts)
  --config FILE         JSON settings (default: ARTIFACTS/configs/repair.json)
  --model NAME          Override the Ollama model
  --execution host|docker  Test execution backend (default: host)
  --image NAME          Preinstalled Docker image for isolated tests
  --issue-file FILE     Read issue text from a file instead of --issue
  --base COMMIT         Repair the committed snapshot (default: HEAD)
  --validation-command '["program","arg"]'  Separate final validator
  --protect '["path"]'  Additional files the model may not edit

Host tests execute with your permissions. Docker mode disables container networking.
Only committed files enter the repair checkout; the source working tree is not edited.
Exit codes: 0 success, 1 repair/diagnostic failure, 2 usage/setup error.
)";
fs::path defaultArtifacts() {
    auto path = fs::current_path();
    while (true) {
        if (fs::exists(path / ".git")) return path / ".Argxx/.artifacts";
        if (path == path.root_path()) break;
        path = path.parent_path();
    }
    return fs::current_path() / ".Argxx/.artifacts";
}
void packageBundle(const fs::path& artifacts, const fs::path& executable) {
    const auto id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    const auto stage = artifacts / "package" / id;
    fs::create_directories(stage);
    for (const auto& folder : {"configs", "prompts", "adapters"}) {
        const auto source = artifacts / folder;
        if (!fs::exists(source)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(source)) {
            if (entry.is_symlink())
                throw std::runtime_error("Refusing to package symlink: " + entry.path().string());
            if (!entry.is_regular_file()) continue;
            const auto target = stage / fs::relative(entry.path(), artifacts);
            fs::create_directories(target.parent_path());
            fs::copy_file(entry.path(), target);
        }
    }
    fs::create_directories(stage / "bin");
    fs::copy_file(executable, stage / "bin/agent");
    auto training = executable.parent_path().parent_path() / "share/argxx/training";
    if (!fs::exists(training)) training = ARGXX_TRAINING_SOURCE_DIR;
    if (fs::exists(training)) {
        fs::create_directories(stage / "training");
        for (const auto* name : {"train_lora.py", "requirements-training.txt"})
            fs::copy_file(training / name, stage / "training" / name);
    }
    writeJson(stage / "agent.json", {{"name", "Argxx"},
                                     {"version", "0.2.0"},
                                     {"runtime", "bin/agent"},
                                     {"config", "configs/repair.json"}});
    writeText(stage / "README.txt",
              "Argxx local configuration bundle, with a native executable for the build host.\n"
              "Requires compatible system CPR, yaml-cpp and their shared libraries; this is not a "
              "portable static binary.\n"
              "Run bin/agent doctor --artifacts . then bin/agent evaluate --artifacts . --manifest "
              "/path/tasks.json\n"
              "Host tests run with your permissions. Use --execution docker --image YOUR_IMAGE for "
              "isolated tests.\n"
              "Training is optional. No trained model is implied by this bundle.\n");
    Json hashes = Json::object();
    for (const auto& entry : fs::recursive_directory_iterator(stage)) {
        if (!entry.is_regular_file()) continue;
        const auto hash = checkedProcess({"cmake", "-E", "sha256sum", entry.path().string()}, stage)
                              .substr(0, 64);
        hashes[fs::relative(entry.path(), stage).generic_string()] = hash;
    }
    writeJson(stage / "manifest.json",
              {{"files", hashes},
               {"trained_adapter_present",
                fs::exists(stage / "adapters/main_lora/adapter_config.json") &&
                    fs::exists(stage / "adapters/main_lora/adapter_model.safetensors")}});
    const auto archive = artifacts / ("argxx-bundle-" + id + ".zip");
    std::vector<std::string> args = {"cmake",          "-E",           "tar", "cf",
                                     archive.string(), "--format=zip", "--"};
    const auto entries = fs::directory_iterator(stage);
    std::transform(fs::begin(entries), fs::end(entries), std::back_inserter(args),
                   [](const auto& entry) { return entry.path().filename().string(); });
    checkedProcess(args, stage, 120);
    std::cout << archive << '\n';
}
int doctor(const Json& config, const fs::path& artifacts) {
    bool ok = true;
    for (const auto* name : {"git", "cmake"}) {
        const auto result = runProcess({name, "--version"}, fs::current_path(), 10);
        const bool found = result.code == 0 && !result.timedOut;
        std::cout << name << ": " << (found ? "available" : "missing") << '\n';
        ok = ok && found;
    }
    auto endpoint = config.at("endpoint").get<std::string>();
    const auto pos = endpoint.rfind("/api/chat");
    if (pos == std::string::npos)
        throw std::runtime_error("doctor expects endpoint ending in /api/chat");
    endpoint.replace(pos, 9, "/api/tags");
    const auto response =
        cpr::Get(cpr::Url{endpoint}, cpr::Timeout{5000}, cpr::ConnectTimeout{3000});
    bool found = false;
    const bool connected = response.error.code == cpr::ErrorCode::OK && response.status_code == 200;
    if (connected) {
        const auto data = Json::parse(response.text);
        for (const auto& model : data.at("models")) {
            const auto name = model.at("name").get<std::string>();
            if (name == config.at("model") ||
                name == config.at("model").get<std::string>() + ":latest")
                found = true;
        }
        std::cout << "Ollama: connected\n";
    } else
        std::cout << "Ollama: connection failed (HTTP " << response.status_code
                  << "): " << response.error.message << ". Check the endpoint and ollama serve.\n";
    std::cout << "Model " << config.at("model") << ": "
              << (found       ? "installed"
                  : connected ? "not installed; use ollama pull"
                              : "not checked")
              << '\n';
    ok = ok && found;
    if (config.value("execution", "host") == "docker") {
        const auto result =
            runProcess({"docker", "image", "inspect", config.at("docker_image").get<std::string>()},
                       fs::current_path(), 15);
        const bool ready = result.code == 0 && !result.timedOut;
        std::cout << "Docker image: " << (ready ? "available" : "unavailable") << '\n';
        ok = ok && ready;
    }
    std::cout << "Test execution: " << config.value("execution", "host")
              << "\nArtifacts: " << artifacts << '\n';
    return ok ? 0 : 1;
}
}  // namespace
int runCli(int argc, char** argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "help") {
            std::cout << help;
            return 0;
        }
        const std::string command = argv[1];
        if (command == "--version") {
            std::cout << "Argxx 0.2.0\n";
            return 0;
        }
        const std::set<std::string> commands = {"init",     "doctor",          "repair", "evaluate",
                                                "fixtures", "export-training", "package"};
        if (!commands.count(command))
            throw std::runtime_error("Unknown command: " + command + "; use --help");
        const std::set<std::string> known = {
            "--artifacts", "--config",  "--model",      "--execution",    "--image",
            "--repo",      "--issue",   "--issue-file", "--test-command", "--validation-command",
            "--base",      "--protect", "--task",       "--manifest",     "--run"};
        std::map<std::string, std::string> flags;
        for (int i = 2; i < argc; i += 2) {
            const std::string key = argv[i];
            if (!known.count(key) || i + 1 == argc || flags.count(key))
                throw std::runtime_error("Unknown, duplicate or incomplete option: " + key);
            flags[key] = argv[i + 1];
        }
        std::set<std::string> allowed = {"--artifacts"};
        if (command == "doctor" || command == "repair" || command == "evaluate")
            allowed.insert({"--config", "--model", "--execution", "--image"});
        if (command == "repair")
            allowed.insert({"--repo", "--issue", "--issue-file", "--test-command",
                            "--validation-command", "--base", "--protect", "--task"});
        if (command == "evaluate") allowed.insert("--manifest");
        if (command == "export-training") allowed.insert("--run");
        const auto invalid = std::find_if(flags.begin(), flags.end(), [&](const auto& flag) {
            return !allowed.count(flag.first);
        });
        if (invalid != flags.end())
            throw std::runtime_error("Option " + invalid->first + " does not apply to " + command);
        if (flags.count("--task"))
            for (const auto* key : {"--repo", "--issue", "--issue-file", "--test-command",
                                    "--validation-command", "--base", "--protect"})
                if (flags.count(key))
                    throw std::runtime_error("Do not combine --task with " + std::string(key));
        const auto require = [&](const std::string& key) {
            if (!flags.count(key) || flags.at(key).empty())
                throw std::runtime_error("Required option: " + key);
            return flags.at(key);
        };
        const auto artifacts = fs::absolute(
            flags.count("--artifacts") ? fs::path(flags.at("--artifacts")) : defaultArtifacts());
        if (command == "init") {
            initialize(artifacts);
            std::cout << artifacts << '\n';
            return 0;
        }
        Json config = defaultRepairConfig();
        const auto configPath = flags.count("--config") ? fs::path(flags.at("--config"))
                                                        : artifacts / "configs/repair.json";
        if (fs::exists(configPath))
            config.update(Json::parse(readText(configPath)));
        else if (flags.count("--config"))
            throw std::runtime_error("Config not found: " + configPath.string());
        if (flags.count("--model")) config["model"] = flags.at("--model");
        if (flags.count("--execution")) config["execution"] = flags.at("--execution");
        if (flags.count("--image")) config["docker_image"] = flags.at("--image");
        if (fs::exists(artifacts / "prompts/system.md"))
            config["system_prompt"] = readText(artifacts / "prompts/system.md", 20000);
        validateConfig(config);
        if (command == "doctor") return doctor(config, artifacts);
        if (command == "fixtures") {
            std::cout << makeFixtures(artifacts) << '\n';
            return 0;
        }
        if (command == "export-training") {
            exportTraining(fs::absolute(require("--run")), artifacts);
            return 0;
        }
        if (command == "package") {
            initialize(artifacts);
            auto executable = fs::absolute(argv[0]);
            if (!fs::exists(executable)) {
                // Installed executables invoked through PATH have no slash in argv[0].
                executable = fs::path(checkedProcess({"which", argv[0]}, fs::current_path()));
                auto s = executable.string();
                while (!s.empty() && s.back() == '\n') s.pop_back();
                executable = s;
            }
            packageBundle(artifacts, fs::canonical(executable));
            return 0;
        }
        Json tasks;
        if (command == "evaluate")
            tasks = Json::parse(readText(require("--manifest")));
        else if (flags.count("--task"))
            tasks = Json::array({Json::parse(readText(flags.at("--task")))});
        else {
            if (flags.count("--issue") && flags.count("--issue-file"))
                throw std::runtime_error("Choose --issue or --issue-file");
            Json task = {
                {"id", "repair"},
                {"repo", fs::absolute(require("--repo")).string()},
                {"issue", flags.count("--issue-file") ? readText(flags.at("--issue-file"), 20000)
                                                      : require("--issue")},
                {"test_command", Json::parse(require("--test-command"))},
                {"base_commit", flags.count("--base") ? flags.at("--base") : "HEAD"}};
            if (flags.count("--protect"))
                task["protected_paths"] = Json::parse(flags.at("--protect"));
            if (flags.count("--validation-command"))
                task["validation_command"] = Json::parse(flags.at("--validation-command"));
            tasks = Json::array({task});
        }
        // Validate before creating output directories.
        if (!tasks.is_array() || tasks.empty())
            throw std::runtime_error("Expected a nonempty task array");
        for (const auto& task : tasks) validateTask(task);
        initialize(artifacts);
        const auto summary = evaluate(tasks, config, artifacts);
        std::cout << summary.dump(2) << '\n';
        return summary.at("passed") == summary.at("tasks") ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Argxx: " << error.what() << '\n';
        return 2;
    }
}
}  // namespace argxx
