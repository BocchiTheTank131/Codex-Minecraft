#pragma once

enum class GameMode {
    Survival,
    Creative,
    Spectator,
};

inline GameMode nextGameMode(GameMode mode) {
    switch (mode) {
    case GameMode::Survival: return GameMode::Creative;
    case GameMode::Creative: return GameMode::Spectator;
    case GameMode::Spectator: return GameMode::Survival;
    }
    return GameMode::Survival;
}

inline const char* gameModeName(GameMode mode) {
    switch (mode) {
    case GameMode::Survival: return "SURVIVAL";
    case GameMode::Creative: return "CREATIVE";
    case GameMode::Spectator: return "SPECTATOR";
    }
    return "SURVIVAL";
}
