#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

const std::string parent_folder = ".Argxx";
const std::string graph_folder = ".graph";
const std::string issue_folder = ".issue";

void createGraphFolder() {
    try {
        if (fs::create_directories(parent_folder + "/" + graph_folder)) {
            std::cout << "Graph Folder created Successfully" << std::endl;
        } else {
            std::cout << "Graph Folder already exist" << std::endl;
        }
    } catch (const fs::filesystem_error& e) {
        std::cout << e.what() << std::endl;
    }
}

void createIssueFolder() {
    try {
        if (fs::create_directories(parent_folder + "/" + issue_folder)) {
            std::cout << "Issue Folder created successfully" << std::endl;
        } else {
            std::cout << "Issue Folder already exist" << std::endl;
        }
    } catch (const fs::filesystem_error& e) {
        std::cout << e.what() << std::endl;
    }
}
