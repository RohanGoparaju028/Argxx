// The purpose of this file to create Gemma4:e4b that is served through ollama for an ai agent
#include "Agent.hpp"
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <iostream>
#include<vector>
#include<unordered_map>

const std::string configfile = "agent.yaml";
const std::vector agent_fields = {"name","description","model","auth","system_prompt","tools"};
void configureModel() {
    if (!std::filesystem::exists(configfile)) {
        std::cerr << configfile << " does not exist" << std::endl;
        return;
    }
    try {
        YAML::Node config = YAML::LoadFile(configfile);
        std::cout << "Loaded " << configfile << " successfully (Work in progress)" << std::endl;
	std::unordered_map<std::string,std::string> agent_info;
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
	 for(auto element: agent_info) {
		 std::cout<<element.first<<" "<<element.second; 
	 }
	 std::cout<<std::endl;
    }

    } catch (const YAML::Exception& e) {
        std::cerr << "Failed to parse " << configfile << ": " << e.what() << std::endl;
    }
}
void createAgent(const std::string& repo) {
    (void)repo;
    configureModel();
}
