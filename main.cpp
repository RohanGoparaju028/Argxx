#include <iostream>

#include "Agent.hpp"
#include "Repair.hpp"

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "chat") {
        if (argc > 3) {
            std::cerr << "Usage: agent chat [owner/repository]\n";
            return 2;
        }
        std::string repo;
        if (argc == 3)
            repo = argv[2];
        else {
            std::cout << "GitHub owner/repository: ";
            std::getline(std::cin, repo);
        }
        if (repo.empty()) return 2;
        return createAgent(repo) ? 0 : 1;
    }
    return argxx::runCli(argc, argv);
}
