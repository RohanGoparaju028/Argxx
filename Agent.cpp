// The purpose of this file to create Gemma4:e4b that is served through ollama for an ai agent
#include "Agent.hpp"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <iostream>

const std::string configfile = "agent.yaml";
void configureModel() {
    if (!std::filesystem::exists(configfile)) {
        std::cerr << configfile << " does not exist" << std::endl;
        return;
    }
    try {
        YAML::Node config = YAML::LoadFile(configfile);
        std::cout << "Loaded " << configfile << " successfully (Work in progress)" << std::endl;
    } catch (const YAML::Exception& e) {
        std::cerr << "Failed to parse " << configfile << ": " << e.what() << std::endl;
    }
}
void createAgent(const std::string& repo) {
    (void)repo;
    configureModel();
}
