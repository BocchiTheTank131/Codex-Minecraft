#include "ChatUI.h"
#include "CommandSystem.h"
#include "InputManager.h"
#include <GLFW/glfw3.h>
#include <algorithm>

void ChatUI::open() {
    open_ = true;
    input_ = "/";
    caret_ = 1;
    historyPosition_ = history_.size();
    draft_.clear();
    suggestions_.clear();
}
void ChatUI::close() { open_ = false; suggestions_.clear(); }
void ChatUI::insert(const std::string& text) {
    for (char c : text) {
        if (c >= 32 && c <= 126 && input_.size() < 256) {
            input_.insert(caret_, 1, c);
            ++caret_;
        }
    }
}
void ChatUI::addMessage(std::string text, ChatTone tone, double now) {
    while (!text.empty()) {
        const std::size_t newline = text.find('\n');
        const std::size_t available = newline == std::string::npos ? text.size() : newline;
        std::size_t length = std::min<std::size_t>(available, 65);
        if (length < available) {
            const std::size_t space = text.rfind(' ', length);
            if (space != std::string::npos && space > 0) length = space;
        }
        messages_.push_back({text.substr(0, length), tone, now});
        text.erase(0, length);
        if (!text.empty() && (text.front() == ' ' || text.front() == '\n')) text.erase(0, 1);
    }
    while (messages_.size() > 100) messages_.pop_front();
}
std::string ChatUI::update(InputManager& input, GLFWwindow* window,
                           const CommandSystem& commands) {
    if (!open_) return {};
    if (input.keyPressed(GLFW_KEY_ESCAPE)) { close(); return {}; }
    if (input.keyPressed(GLFW_KEY_ENTER) || input.keyPressed(GLFW_KEY_KP_ENTER)) {
        std::string submitted = input_;
        if (submitted.size() > 1) {
            if (history_.empty() || history_.back() != submitted) history_.push_back(submitted);
            while (history_.size() > 100) history_.pop_front();
        }
        close();
        return submitted;
    }
    if (input.keyPressed(GLFW_KEY_UP) && !history_.empty()) {
        if (historyPosition_ == history_.size()) draft_ = input_;
        if (historyPosition_ > 0) --historyPosition_;
        input_ = history_[historyPosition_]; caret_ = input_.size();
    }
    if (input.keyPressed(GLFW_KEY_DOWN) && historyPosition_ < history_.size()) {
        ++historyPosition_;
        input_ = historyPosition_ == history_.size() ? draft_ : history_[historyPosition_];
        caret_ = input_.size();
    }
    if (input.keyPressed(GLFW_KEY_LEFT) && caret_ > 0) --caret_;
    if (input.keyPressed(GLFW_KEY_RIGHT) && caret_ < input_.size()) ++caret_;
    if (input.keyPressed(GLFW_KEY_HOME)) caret_ = 0;
    if (input.keyPressed(GLFW_KEY_END)) caret_ = input_.size();
    if (input.keyPressed(GLFW_KEY_BACKSPACE) && caret_ > 0) {
        input_.erase(--caret_, 1);
    }
    if (input.keyPressed(GLFW_KEY_DELETE) && caret_ < input_.size()) input_.erase(caret_, 1);
    if ((input.keyDown(GLFW_KEY_LEFT_CONTROL) || input.keyDown(GLFW_KEY_RIGHT_CONTROL)) &&
        input.keyPressed(GLFW_KEY_V)) {
        if (const char* clipboard = glfwGetClipboardString(window)) insert(clipboard);
    } else {
        std::string characters = input.consumeTypedCharacters();
        // GLFW may deliver the character after the key event that opened chat.
        if (input_ == "/" && caret_ == 1 && !characters.empty() && characters.front() == '/')
            characters.erase(0, 1);
        insert(characters);
    }
    suggestions_ = commands.suggest(input_);
    if (input.keyPressed(GLFW_KEY_TAB) && !suggestions_.empty()) {
        const std::size_t start = input_.find_last_of(' ');
        const std::size_t begin = start == std::string::npos ? 1 : start + 1;
        input_.replace(begin, input_.size() - begin, suggestions_.front());
        caret_ = input_.size();
        suggestions_ = commands.suggest(input_);
    }
    return {};
}
bool ChatUI::runSelfTest(std::string& report) {
    ChatUI chat; chat.open();
    if (!chat.isOpen() || chat.input() != "/" || chat.caret() != 1) { report="chat open"; return false; }
    chat.insert("gamemode creative");
    if (chat.input() != "/gamemode creative") { report="chat insert"; return false; }
    chat.close();
    if (chat.isOpen()) { report="chat close"; return false; }
    report = "chat input OK"; return true;
}
