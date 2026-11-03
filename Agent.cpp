// The purpose of this file to create Gemma4:e4b that is served through ollama for an ai agent
#include "Agent.hpp"
#include<algorithm>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <vector>

const std::string configfile = "agent.yaml";
const std::vector agent_fields = {"name", "description", "model", "auth", "system_prompt", "tools"};
std::unordered_map<std::string, std::string> configureModel() {
    if (!std::filesystem::exists(configfile)) {
        std::cerr << configfile << " does not exist" << std::endl;
        return {};
    }
    try {
        YAML::Node config = YAML::LoadFile(configfile);
        std::cout << "Loaded " << configfile << " successfully (Work in progress)" << std::endl;
        std::unordered_map<std::string, std::string> agent_info;
        for (const auto& field : agent_fields) {
            YAML::Node node = config["agent"][field];
            if (!node.IsDefined()) continue;

            if (node.IsScalar()) {
                agent_info[field] = node.as<std::string>();
            } else {
                YAML::Emitter emitter;
                emitter << node;
                agent_info[field] = emitter.c_str();
            }
        }
	YAML::Emitter emitter;
	emitter << agent_info["tools"];
	std::string str = emitter.c_str();
	str.erase(std::remove(str.begin(),str.end(),'-'),str.end());
	auto it = std::find(str.begin(),str.end(),' ');
	if(it != str.end()) {
	  str.erase(it);
	}
	size_t pos = 0;
        while ((pos = str.find("\\n", pos)) != std::string::npos) {
            str.erase(pos, 2); 
        }
	agent_info["tools"] = str;
        return agent_info;
    } catch (const YAML::Exception& e) {
        std::cerr << "Failed to parse " << configfile << ": " << e.what() << std::endl;
        return {};
    }
}
void createAgent(const std::string& repo) {
    (void)repo;
    auto agent_details = configureModel();

    if (agent_details.empty()) return;
}
