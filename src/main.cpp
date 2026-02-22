#include "Application.h"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        std::cout << "Starting Horizon Unseen..." << std::endl;
        Application app;
        std::cout << "Application initialized successfully!" << std::endl;
        app.run();
        std::cout << "Application exited normally." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return EXIT_FAILURE;
    }

    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    return EXIT_SUCCESS;
}
