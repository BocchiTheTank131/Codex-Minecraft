#include "Game.h"
#ifdef VOXEL_STANDALONE
#include "UserData.h"
#include <iostream>
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#endif

#include <cstdlib>

int main(int argc, char** argv) {
#ifdef VOXEL_STANDALONE
    try {
        UserData::initialize();
    } catch (const std::exception& error) {
        std::cerr << "Cannot open Voxel Frontier user data: " << error.what() << '\n';
        MessageBoxA(nullptr, error.what(), "Voxel Frontier - User Data Error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
#endif
    Game game;
    if (!game.initialize(argc, argv)) {
        return EXIT_FAILURE;
    }

    const int result = game.run();
    game.shutdown();
    return result;
}
