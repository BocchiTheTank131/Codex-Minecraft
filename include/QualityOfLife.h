#pragma once
#include "Survival.h"
#include <string>
#include <vector>

// Shared categories for sorting, the Creative catalog and future content.
int itemCategory(Item item);
const char* itemCategoryName(int category);
struct RecipeFeedback {bool craftable=false;bool needsTable=false;std::vector<std::string> missing;std::string message;};
struct PlacementBlock {glm::ivec3 position{0};Block block=Block::Air;};
struct PlacementPlan {std::vector<PlacementBlock> blocks;bool valid=false;std::string reason;};
RecipeFeedback recipeFeedback(const Inventory&,int recipe,bool table);
struct SaveFeedback {
    enum class Phase {Idle,Pending,Saved,Failed};
    Phase phase=Phase::Idle;
    double changed=0,lastSuccess=-1;
    std::string time;
    void pending(double now){phase=Phase::Pending;changed=now;}
    void complete(bool success,double now);
    std::string label(double now,bool pause=false)const;
};

// ASCII sign format remains four lines of at most 24 characters (99 bytes).
struct SignEditor {
    std::string text;
    std::size_t caret=0,anchor=0;
    void begin(const std::string& value);
    bool insert(const std::string& value);
    void move(int direction,bool select=false,bool byLine=false);
    void home(bool end,bool select=false);
    void erase(bool forward);
    std::string selected() const;
    static bool valid(const std::string&);
};
