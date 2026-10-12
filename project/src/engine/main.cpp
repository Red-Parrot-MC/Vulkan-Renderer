#include "../project/Application.h"
#include <iostream>

int main() {
    BaseApplication* app = new Application();

    try {
        app->run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}