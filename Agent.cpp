// The purpose of this file to create Gemma4:e4b that is served through ollama for an ai agent 
#include<iostream>
#include<filesystem>
#include<yaml-cpp/yaml.h>
#include "Agent.hpp"


const std::string configfile = "agent.yaml";
void configureModel() {
   if(std::filesystem::exists(configfile)) {
	   std::cout<<configfile<<" does not exist"<<std::endl;
	   return;
   }
   YAML::Node config = YAML::Load(configfile);
   std::cout<<"Work in progress"<<std::endl;
}
void createAgent(std::string repo) {
   configureModel();
}
