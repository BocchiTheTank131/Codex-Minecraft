#include "Game.h"

#include <cstdlib>

int main(int argc, char** argv) {
    Game game;
    if (!game.initialize(argc, argv)) {
        return EXIT_FAILURE;
    }

    const int result = game.run();
    game.shutdown();
    return result;
}
