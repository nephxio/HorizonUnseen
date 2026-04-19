#include "EditorApplication.h"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        EditorApplication app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Editor fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
