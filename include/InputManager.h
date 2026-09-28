#pragma once

#include <glm/glm.hpp>

#include <array>
#include <string>

struct GLFWwindow;
struct PlayerInput;

enum class ControlAction : int {
    Forward, Backward, Left, Right, Jump, Sprint, Sneak,
    Inventory, Drop, PickBlock, Zoom, Fullbright, Debug, Screenshot,
    Count
};
inline constexpr int ControlActionCount = static_cast<int>(ControlAction::Count);
using ControlBindings = std::array<int, ControlActionCount>;
ControlBindings defaultControlBindings();
const char* controlActionName(ControlAction action);
std::string controlKeyName(int key);

class InputManager {
public:
    void attach(GLFWwindow* window);
    void pollEvents();

    bool keyDown(int key) const;
    bool keyPressed(int key) const;
    bool mouseDown(int button) const;
    bool mousePressed(int button) const;
    bool actionDown(ControlAction action) const;
    bool actionPressed(ControlAction action) const;
    int firstPressedKey() const;
    void setBindings(const ControlBindings& bindings) { bindings_ = bindings; }

    glm::dvec2 consumeMouseDelta();
    double consumeScrollDelta();
    std::string consumeTypedCharacters();
    glm::dvec2 framebufferCursorPosition() const;
    PlayerInput playerInput(bool enabled) const;

    void setCursorCaptured(bool captured);
    bool cursorCaptured() const {
        return cursorCaptured_;
    }

private:
    static constexpr int KeyCount = 512;
    static constexpr int MouseButtonCount = 8;

    GLFWwindow* window_ = nullptr;
    std::array<bool, KeyCount> keys_{};
    std::array<bool, KeyCount> previousKeys_{};
    std::array<bool, MouseButtonCount> mouseButtons_{};
    std::array<bool, MouseButtonCount> previousMouseButtons_{};

    glm::dvec2 mouseDelta_{0.0};
    glm::dvec2 lastMousePosition_{0.0};
    double scrollDelta_ = 0.0;
    std::string typedCharacters_;
    bool cursorCaptured_ = false;
    bool firstMouseSample_ = true;
    ControlBindings bindings_ = defaultControlBindings();

    static void cursorPositionCallback(GLFWwindow* window, double x, double y);
    static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);
    static void characterCallback(GLFWwindow* window, unsigned int codepoint);
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);

    void onCursorPosition(double x, double y);
    void onScroll(double yOffset);
};
