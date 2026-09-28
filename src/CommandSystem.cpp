#include "CommandSystem.h"
#include "Definitions.h"
#include "Player.h"
#include "Survival.h"
#include "World.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>

namespace {
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}
std::string canonical(std::string value) {
    value = lower(value);
    if (value.rfind("minecraft:", 0) == 0) value.erase(0, 10);
    value.erase(std::remove_if(value.begin(), value.end(), [](char c) {
        return c == '_' || c == '-' || c == ' ';
    }), value.end());
    return value;
}
CommandResult ok(std::string text) { return {ChatTone::Success, std::move(text)}; }
CommandResult error(std::string text) { return {ChatTone::Error, std::move(text)}; }
bool real(const std::string& text, double& value) {
    if (text.empty()) return false;
    char* end = nullptr;
    value = std::strtod(text.c_str(), &end);
    return end == text.c_str() + text.size() && std::isfinite(value);
}
bool integer(const std::string& text, int& value) {
    double parsed = 0;
    if (!real(text, parsed) || parsed < static_cast<double>(std::numeric_limits<int>::min()) ||
        parsed > static_cast<double>(std::numeric_limits<int>::max()) || std::floor(parsed) != parsed)
        return false;
    value = static_cast<int>(parsed); return true;
}
bool coordinate(const std::string& text, double base, double& result) {
    if (text == "~") { result = base; return true; }
    if (text.size() > 1 && text.front() == '~') {
        double offset = 0;
        if (!real(text.substr(1), offset)) return false;
        result = base + offset;
    } else if (!real(text, result)) return false;
    return result >= -30000000.0 && result <= 30000000.0;
}
bool position(const std::vector<std::string>& a, std::size_t offset,
              const glm::vec3& base, glm::dvec3& result) {
    return a.size() >= offset + 3 &&
        coordinate(a[offset], base.x, result.x) &&
        coordinate(a[offset + 1], base.y, result.y) &&
        coordinate(a[offset + 2], base.z, result.z);
}
Item findItem(const std::string& name) {
    const std::string wanted = canonical(name);
    for (int id = 1; id < static_cast<int>(Item::Count); ++id) {
        const Item item = static_cast<Item>(id);
        if (canonical(itemDefinition(item).displayName) == wanted) return item;
    }
    return Item::None;
}
bool findBlock(const std::string& name, Block& block) {
    if (canonical(name) == "air") { block = Block::Air; return true; }
    const Item item = findItem(name);
    if (item == Item::None) return false;
    block = itemToBlock(item);
    return block != Block::Air;
}
std::string modeName(GameMode mode) {
    switch (mode) {
    case GameMode::Survival: return "Survival";
    case GameMode::Creative: return "Creative";
    case GameMode::Spectator: return "Spectator";
    }
    return "Unknown";
}
const std::vector<std::string>& mobNames() {
    static const std::vector<std::string> names{
        "cow", "pig", "sheep", "villager", "pillager",
        "HitoriGotoh", "KitaIkuyo", "NijikaIjichi", "RyoYamada"};
    return names;
}
} // namespace

bool CommandSystem::tokenize(const std::string& line, std::vector<std::string>& tokens) {
    tokens.clear(); std::string current; bool quoted = false, active = false;
    for (char c : line) {
        if (c == '"') { quoted = !quoted; active = true; }
        else if (std::isspace(static_cast<unsigned char>(c)) && !quoted) {
            if (active) { tokens.push_back(current); current.clear(); active = false; }
        } else { current.push_back(c); active = true; }
    }
    if (quoted) return false;
    if (active) tokens.push_back(current);
    return true;
}
const CommandSystem::Definition* CommandSystem::find(const std::string& name) const {
    const std::string needle = lower(name);
    for (const Definition& definition : definitions_) {
        if (definition.name == needle || (!definition.aliases.empty() && definition.aliases == needle))
            return &definition;
    }
    return nullptr;
}
CommandSystem::CommandSystem() {
    definitions_.push_back({"help", "", "/help [command]", "Show command help", false,
        [this](const auto& a, CommandContext&) -> CommandResult {
            if (a.size() > 2) return error("Usage: /help [command]");
            if (a.size() == 2) {
                const Definition* command = find(a[1]);
                return command ? CommandResult{ChatTone::Normal, command->usage + " - " + command->description}
                               : error("Unknown command: " + a[1]);
            }
            return {ChatTone::Normal,
                "Commands: help, gamemode, time, give, clear, tp, kill, summon, seed, setblock, fill"};
        }});
    definitions_.push_back({"gamemode", "", "/gamemode <survival|creative|spectator>",
        "Change game mode", true, [](const auto& a, CommandContext& c) -> CommandResult {
            if (a.size() != 2) return error("Usage: /gamemode <survival|creative|spectator>");
            const std::string mode = lower(a[1]);
            GameMode selected;
            if (mode == "survival" || mode == "s") selected = GameMode::Survival;
            else if (mode == "creative" || mode == "c") selected = GameMode::Creative;
            else if (mode == "spectator") selected = GameMode::Spectator;
            else return error("Unknown game mode: " + a[1]);
            c.setGameMode(selected); return ok("Game mode set to " + modeName(selected));
        }});
    definitions_.push_back({"time", "", "/time <set <day|noon|night|midnight|seconds>|add <seconds>>",
        "Set or advance the 420-second day cycle", true,
        [](const auto& a, CommandContext& c) -> CommandResult {
            if (a.size() != 3) return error("Usage: /time <set|add> <value>");
            double value = 0;
            const std::string word = lower(a[2]);
            if (lower(a[1]) == "set") {
                if (word == "day") value = 35;
                else if (word == "noon") value = 105;
                else if (word == "night") value = 245;
                else if (word == "midnight") value = 315;
                else if (!real(a[2], value)) return error("Invalid time: " + a[2]);
            } else if (lower(a[1]) == "add") {
                if (!real(a[2], value)) return error("Invalid seconds: " + a[2]);
                value += c.worldTime;
            } else return error("Usage: /time <set|add> <value>");
            c.worldTime = static_cast<float>(std::fmod(std::fmod(value, 420.0) + 420.0, 420.0));
            if (lower(a[1]) == "set" &&
                (word == "day" || word == "noon" || word == "night" || word == "midnight")) {
                std::string label = word;
                label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
                return ok("Time set to " + label);
            }
            return ok("Time set to " + std::to_string(static_cast<int>(c.worldTime)) + " seconds");
        }});
    definitions_.push_back({"give", "", "/give [@s] <item> [count]", "Give an item", true,
        [](const auto& a, CommandContext& c) -> CommandResult {
            std::size_t index = 1;
            if (a.size() > 1 && a[1].front() == '@') {
                if (a[1] != "@s" && a[1] != "@p") return error("Only @s and @p are supported");
                ++index;
            }
            if (a.size() < index + 1 || a.size() > index + 2)
                return error("Usage: /give [@s] <item> [count]");
            const Item item = findItem(a[index]);
            if (item == Item::None) return error("Unknown item: " + a[index]);
            int count = 1;
            if (a.size() == index + 2 && (!integer(a[index + 1], count) || count < 1 || count > 2304))
                return error("Count must be 1-2304");
            const int leftover = c.inventory.add(item, count);
            const int inserted = count - leftover;
            std::string message = "Gave " + std::to_string(inserted) + " " + itemDefinition(item).displayName;
            if (leftover) message += "; " + std::to_string(leftover) + " could not fit";
            return {leftover ? ChatTone::Warning : ChatTone::Success, message};
        }});
    definitions_.push_back({"clear", "", "/clear [item] [count]", "Clear inventory items", true,
        [](const auto& a, CommandContext& c) -> CommandResult {
            if (a.size() == 1) { c.inventory.clear(); return ok("Inventory cleared"); }
            if (a.size() > 3) return error("Usage: /clear [item] [count]");
            const Item item = findItem(a[1]);
            if (item == Item::None) return error("Unknown item: " + a[1]);
            int count = c.inventory.count(item);
            if (a.size() == 3 && (!integer(a[2], count) || count < 1))
                return error("Count must be a positive integer");
            count = std::min(count, c.inventory.count(item));
            if (count > 0) c.inventory.remove(item, count);
            return ok("Removed " + std::to_string(count) + " " + itemDefinition(item).displayName);
        }});
    definitions_.push_back({"tp", "teleport", "/tp <x> <y> <z>", "Teleport the player", true,
        [](const auto& a, CommandContext& c) -> CommandResult {
            glm::dvec3 destination;
            if (a.size() != 4 || !position(a, 1, c.player.position(), destination))
                return error("Usage: /tp <x> <y> <z>");
            if (destination.y < 0 || destination.y >= WORLD_HEIGHT)
                return error("Y must be inside the world");
            c.player.teleport(glm::vec3(destination));
            return ok("Teleported to " + std::to_string(static_cast<int>(destination.x)) + " " +
                      std::to_string(static_cast<int>(destination.y)) + " " +
                      std::to_string(static_cast<int>(destination.z)));
        }});
    definitions_.push_back({"kill", "", "/kill [@s]", "Kill the current player", true,
        [](const auto& a, CommandContext& c) -> CommandResult {
            if (a.size() > 2 || (a.size() == 2 && a[1] != "@s" && a[1] != "@p"))
                return error("Usage: /kill [@s]");
            c.player.killByCommand(); return ok("Killed player");
        }});
    definitions_.push_back({"summon", "", "/summon <mob> [x y z]", "Summon a mob", true,
        [](const auto& a, CommandContext& c) -> CommandResult {
            if (a.size() != 2 && a.size() != 5) return error("Usage: /summon <mob> [x y z]");
            glm::dvec3 destination(c.player.position() + c.player.lookDirection() * 3.0f);
            if (a.size() == 5 && !position(a, 2, c.player.position(), destination))
                return error("Usage: /summon <mob> [x y z]");
            if (destination.y < 0 || destination.y >= WORLD_HEIGHT) return error("Y must be inside the world");
            if (!c.survival.summonMob(a[1], glm::vec3(destination))) return error("Unknown mob: " + a[1]);
            return ok("Summoned " + a[1]);
        }});
    definitions_.push_back({"seed", "", "/seed", "Show the current world seed", false,
        [](const auto& a, CommandContext& c) -> CommandResult {
            if (a.size() != 1) return error("Usage: /seed");
            return {ChatTone::Normal, "Seed: " + std::to_string(c.seed)};
        }});
    definitions_.push_back({"setblock", "", "/setblock <x> <y> <z> <block>",
        "Set one world block", true, [](const auto& a, CommandContext& c) -> CommandResult {
            glm::dvec3 point;
            if (a.size() != 5 || !position(a, 1, c.player.position(), point))
                return error("Usage: /setblock <x> <y> <z> <block>");
            Block block;
            if (!findBlock(a[4], block)) return error("Unknown block: " + a[4]);
            const glm::ivec3 cell(glm::floor(point));
            if (cell.y < 0 || cell.y >= WORLD_HEIGHT) return error("Y must be inside the world");
            if (!c.world.hasLoadedChunkAt(cell.x, cell.z)) return error("Target chunk is not loaded");
            c.world.setBlock(cell.x, cell.y, cell.z, block);
            return ok("Block set");
        }});
    definitions_.push_back({"fill", "", "/fill <x1> <y1> <z1> <x2> <y2> <z2> <block>",
        "Fill up to 4096 blocks", true, [](const auto& a, CommandContext& c) -> CommandResult {
            glm::dvec3 first, second;
            if (a.size() != 8 || !position(a, 1, c.player.position(), first) ||
                !position(a, 4, c.player.position(), second))
                return error("Usage: /fill <x1> <y1> <z1> <x2> <y2> <z2> <block>");
            Block block;
            if (!findBlock(a[7], block)) return error("Unknown block: " + a[7]);
            glm::ivec3 low = glm::min(glm::ivec3(glm::floor(first)), glm::ivec3(glm::floor(second)));
            glm::ivec3 high = glm::max(glm::ivec3(glm::floor(first)), glm::ivec3(glm::floor(second)));
            const std::int64_t dx = static_cast<std::int64_t>(high.x) - low.x + 1;
            const std::int64_t dy = static_cast<std::int64_t>(high.y) - low.y + 1;
            const std::int64_t dz = static_cast<std::int64_t>(high.z) - low.z + 1;
            if (dx > 4096 || dy > 4096 || dz > 4096)
                return error("Fill exceeds 4096 blocks");
            const std::int64_t count = dx * dy * dz;
            if (count > 4096) return error("Fill exceeds 4096 blocks (" + std::to_string(count) + ")");
            if (low.y < 0 || high.y >= WORLD_HEIGHT) return error("Y must be inside the world");
            for (int x = low.x; x <= high.x; ++x)
                for (int z = low.z; z <= high.z; ++z)
                    if (!c.world.hasLoadedChunkAt(x, z))
                        return error("Fill region contains an unloaded chunk");
            const int changed = c.world.fillBlocks(low, high, block);
            return ok("Filled " + std::to_string(changed) + " blocks");
        }});
}
CommandResult CommandSystem::execute(const std::string& line, CommandContext& context) const {
    if (line.empty() || line.front() != '/') return error("Commands must begin with /");
    std::vector<std::string> tokens;
    if (!tokenize(line.substr(1), tokens)) return error("Unclosed quote");
    if (tokens.empty()) return error("Enter a command after /");
    const Definition* definition = find(tokens.front());
    if (!definition) return error("Unknown command: " + tokens.front());
    if (definition->cheat && !context.cheatsEnabled)
        return error("Cheats are not enabled in this world.");
    return definition->handler(tokens, context);
}
std::vector<std::string> CommandSystem::suggest(const std::string& line) const {
    if (line.empty() || line.front() != '/') return {};
    const std::size_t lastSpace = line.find_last_of(' ');
    const std::string prefix = lower(lastSpace == std::string::npos ? line.substr(1) : line.substr(lastSpace + 1));
    std::vector<std::string> pool;
    if (lastSpace == std::string::npos) {
        for (const auto& command : definitions_) pool.push_back(command.name);
    } else {
        std::vector<std::string> words;
        tokenize(line.substr(1, lastSpace - 1), words);
        const std::string command = words.empty() ? "" : lower(words[0]);
        if (command == "gamemode") pool = {"survival", "creative", "spectator"};
        else if (command == "time") pool = words.size() == 1
            ? std::vector<std::string>{"set", "add"}
            : std::vector<std::string>{"day", "noon", "night", "midnight"};
        else if (command == "summon") pool = mobNames();
        else if (command == "give" || command == "clear" || command == "setblock" || command == "fill") {
            if (command == "give") pool.push_back("@s");
            if (command == "setblock" || command == "fill") pool.push_back("air");
            for (int id = 1; id < static_cast<int>(Item::Count); ++id) {
                const Item item = static_cast<Item>(id);
                if ((command == "setblock" || command == "fill") && itemToBlock(item) == Block::Air) continue;
                std::string name = lower(itemDefinition(item).displayName);
                std::replace(name.begin(), name.end(), ' ', '_');
                pool.push_back(name);
            }
        } else if (command == "help") {
            for (const auto& definition : definitions_) pool.push_back(definition.name);
        }
    }
    std::vector<std::string> result;
    for (const std::string& candidate : pool) {
        if (lower(candidate).rfind(prefix, 0) == 0) result.push_back(candidate);
        if (result.size() == 8) break;
    }
    return result;
}
bool CommandSystem::runSelfTest(std::string& report) {
    CommandSystem system; std::vector<std::string> tokens;
    if (!tokenize("give @s \"stone bricks\" 64", tokens) || tokens.size() != 4 ||
        tokens[2] != "stone bricks") { report = "quoted tokenizer"; return false; }
    if (tokenize("give \"unfinished", tokens)) { report = "unclosed quote"; return false; }
    if (system.suggest("/gam").empty() || system.suggest("/gam")[0] != "gamemode" ||
        system.suggest("/gamemode c").empty() || system.suggest("/gamemode c")[0] != "creative") {
        report = "autocomplete"; return false;
    }
    double relative = 0;
    if (!coordinate("~-10", 20, relative) || relative != 10 || coordinate("banana", 0, relative)) {
        report = "coordinates"; return false;
    }
    int count = 0;
    if (integer("999999999999", count) || integer("1.5", count)) { report = "integer bounds"; return false; }
    report = "parser and autocomplete OK"; return true;
}
