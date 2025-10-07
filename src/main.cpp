#include "core/application.hpp"
#include <iostream>

auto main(i32 argc, char* argv[]) -> i32 {
    vgce::core::Application app;
    try {
        return app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "A critical error occurred: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "An unknown critical error occurred." << std::endl;
        return 1;
    }
}
