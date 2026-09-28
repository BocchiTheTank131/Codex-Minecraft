#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <vector>

class InputManager;
class CommandSystem;
struct GLFWwindow;

enum class ChatTone { Normal, Success, Warning, Error };
struct ChatMessage { std::string text; ChatTone tone; double time; };

class ChatUI {
public:
    void open();
    void close();
    bool isOpen() const { return open_; }
    // Returns a submitted command, or an empty string when nothing was submitted.
    std::string update(InputManager& input, GLFWwindow* window,
                       const CommandSystem& commands);
    void addMessage(std::string text, ChatTone tone, double now);
    const std::deque<ChatMessage>& messages() const { return messages_; }
    const std::string& input() const { return input_; }
    std::size_t caret() const { return caret_; }
    const std::vector<std::string>& suggestions() const { return suggestions_; }
    static bool runSelfTest(std::string& report);
private:
    bool open_ = false;
    std::string input_;
    std::size_t caret_ = 0;
    std::deque<ChatMessage> messages_;
    std::deque<std::string> history_;
    std::size_t historyPosition_ = 0;
    std::string draft_;
    std::vector<std::string> suggestions_;
    void insert(const std::string& text);
};
