#include "game.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    Game game;
    game.Init();

    if (argc >= 2) {
        std::string command = argv[1];
        if (command == "--screenshot") {
            std::string mode = argc >= 3 ? argv[2] : "battle";
            std::string path = argc >= 4 ? argv[3] : "screenshots/battle.png";
            bool ok = game.SaveScreenshot(mode, path);
            game.Shutdown();
            if (!ok) {
                std::cerr << "Failed to save screenshot: " << path << "\n";
                return 2;
            }
            std::cout << "Saved screenshot: " << path << "\n";
            return 0;
        }
        if (command == "--verify") {
            std::string report;
            bool ok = game.RunVerification(&report);
            game.Shutdown();
            std::cout << report;
            return ok ? 0 : 3;
        }
    }

    while (!game.ShouldClose()) {
        game.Tick();
    }
    game.Shutdown();
    return 0;
}
