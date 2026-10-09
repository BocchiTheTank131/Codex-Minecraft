#pragma once

#include "ChatUI.h"
#include "GameMode.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class World;
class Player;
class Inventory;
class SurvivalWorld;
class Weather;
class SoundSystem;

struct CommandContext {
    World& world;
    Player& player;
    Inventory& inventory;
    SurvivalWorld& survival;
    float& worldTime;
    std::uint32_t seed;
    std::function<GameMode()> gameMode;
    std::function<void(GameMode)> setGameMode;
    bool cheatsEnabled = true; // World creation UI can expose this later.
    Weather* weather = nullptr;
    SoundSystem* sounds = nullptr;
};
struct CommandResult { ChatTone tone = ChatTone::Normal; std::string text; };

class CommandSystem {
public:
    CommandSystem();
    CommandResult execute(const std::string& line, CommandContext& context) const;
    std::vector<std::string> suggest(const std::string& line) const;
    static bool tokenize(const std::string& line, std::vector<std::string>& tokens);
    static bool runSelfTest(std::string& report);
private:
    struct Definition {
        std::string name, aliases, usage, description;
        bool cheat = true;
        std::function<CommandResult(const std::vector<std::string>&, CommandContext&)> handler;
    };
    std::vector<Definition> definitions_;
    const Definition* find(const std::string& name) const;
};
