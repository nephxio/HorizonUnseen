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
        std::cin.get(); // Wait for user input before closing
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
