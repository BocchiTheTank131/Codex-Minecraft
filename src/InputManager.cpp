#include "InputManager.h"

#include "Player.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

ControlBindings defaultControlBindings() {
    return {GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D,
            GLFW_KEY_SPACE, GLFW_KEY_LEFT_CONTROL, GLFW_KEY_LEFT_SHIFT,
            GLFW_KEY_E, GLFW_KEY_Q, GLFW_KEY_R, GLFW_KEY_C,
            GLFW_KEY_G, GLFW_KEY_F3, GLFW_KEY_F2};
}

const char* controlActionName(ControlAction action) {
    static constexpr std::array<const char*, ControlActionCount> names{
        "FORWARD", "BACKWARD", "LEFT", "RIGHT", "JUMP", "SPRINT", "SNEAK",
        "INVENTORY", "DROP", "PICK BLOCK", "ZOOM", "FULLBRIGHT", "DEBUG", "SCREENSHOT"};
    const int index = static_cast<int>(action);
    return index >= 0 && index < ControlActionCount ? names[static_cast<std::size_t>(index)] : "?";
}

std::string controlKeyName(int key) {
    if (const char* name = glfwGetKeyName(key, 0))
        return name;
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25)
        return "F" + std::to_string(key - GLFW_KEY_F1 + 1);
    switch (key) {
    case GLFW_KEY_SPACE: return "SPACE";
    case GLFW_KEY_LEFT_SHIFT: return "LEFT SHIFT";
    case GLFW_KEY_RIGHT_SHIFT: return "RIGHT SHIFT";
    case GLFW_KEY_LEFT_CONTROL: return "LEFT CTRL";
    case GLFW_KEY_RIGHT_CONTROL: return "RIGHT CTRL";
    case GLFW_KEY_TAB: return "TAB";
    case GLFW_KEY_ENTER: return "ENTER";
    case GLFW_KEY_BACKSPACE: return "BACKSPACE";
    default: return "KEY " + std::to_string(key);
    }
}

void InputManager::attach(GLFWwindow* window) {
    window_ = window;
    glfwSetWindowUserPointer(window_, this);
    glfwSetCursorPosCallback(window_, cursorPositionCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetCharCallback(window_, characterCallback);
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
}

void InputManager::pollEvents() {
    previousKeys_ = keys_;
    previousMouseButtons_ = mouseButtons_;
    mouseDelta_ = glm::dvec2(0.0);
    scrollDelta_ = 0.0;
    typedCharacters_.clear();

    glfwPollEvents();

    for (int key = 0; key <= GLFW_KEY_LAST && key < KeyCount; ++key) {
        keys_[key] = glfwGetKey(window_, key) == GLFW_PRESS;
    }
    for (int button = 0; button <= GLFW_MOUSE_BUTTON_LAST && button < MouseButtonCount; ++button) {
        mouseButtons_[button] = glfwGetMouseButton(window_, button) == GLFW_PRESS;
    }
}

bool InputManager::keyDown(int key) const {
    return key >= 0 && key < KeyCount && keys_[key];
}

bool InputManager::keyPressed(int key) const {
    return key >= 0 && key < KeyCount && keys_[key] && !previousKeys_[key];
}

bool InputManager::actionDown(ControlAction action) const {
    return keyDown(bindings_[static_cast<std::size_t>(action)]);
}

bool InputManager::actionPressed(ControlAction action) const {
    return keyPressed(bindings_[static_cast<std::size_t>(action)]);
}

int InputManager::firstPressedKey() const {
    for (int key = 0; key <= GLFW_KEY_LAST && key < KeyCount; ++key) {
        if (keyPressed(key)) return key;
    }
    return -1;
}

bool InputManager::mouseDown(int button) const {
    return button >= 0 && button < MouseButtonCount && mouseButtons_[button];
}

bool InputManager::mousePressed(int button) const {
    return button >= 0 && button < MouseButtonCount && mouseButtons_[button] &&
           !previousMouseButtons_[button];
}

glm::dvec2 InputManager::consumeMouseDelta() {
    const glm::dvec2 result = mouseDelta_;
    mouseDelta_ = glm::dvec2(0.0);
    return result;
}

double InputManager::consumeScrollDelta() {
    const double result = scrollDelta_;
    scrollDelta_ = 0.0;
    return result;
}

std::string InputManager::consumeTypedCharacters() {
    std::string result = std::move(typedCharacters_);
    typedCharacters_.clear();
    return result;
}

void InputManager::characterCallback(GLFWwindow* window, unsigned int codepoint) {
    auto* input = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (input && codepoint >= 32 && codepoint <= 126)
        input->typedCharacters_.push_back(static_cast<char>(codepoint));
}

glm::dvec2 InputManager::framebufferCursorPosition() const {
    double mouseX = 0.0;
    double mouseY = 0.0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    int windowWidth = 0;
    int windowHeight = 0;

    glfwGetCursorPos(window_, &mouseX, &mouseY);
    glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);
    glfwGetWindowSize(window_, &windowWidth, &windowHeight);

    if (windowWidth > 0) {
        mouseX *= static_cast<double>(framebufferWidth) / windowWidth;
    }
    if (windowHeight > 0) {
        mouseY *= static_cast<double>(framebufferHeight) / windowHeight;
    }
    return {mouseX, mouseY};
}

PlayerInput InputManager::playerInput(bool enabled) const {
    PlayerInput input;
    input.enabled = enabled;
    if (!enabled) {
        return input;
    }

    input.moveForward = actionDown(ControlAction::Forward);
    input.moveBackward = actionDown(ControlAction::Backward);
    input.moveLeft = actionDown(ControlAction::Left);
    input.moveRight = actionDown(ControlAction::Right);
    input.jump = actionDown(ControlAction::Jump);
    input.sprint = actionDown(ControlAction::Sprint);
    input.sneak = actionDown(ControlAction::Sneak);
    return input;
}
void InputManager::setCursorCaptured(bool captured) {
    if (cursorCaptured_ == captured) {
        return;
    }
    cursorCaptured_ = captured;
    firstMouseSample_ = true;
    glfwSetInputMode(window_, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void InputManager::cursorPositionCallback(GLFWwindow* window, double x, double y) {
    auto* input = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (input != nullptr) {
        input->onCursorPosition(x, y);
    }
}

void InputManager::scrollCallback(GLFWwindow* window, double, double yOffset) {
    auto* input = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (input != nullptr) {
        input->onScroll(yOffset);
    }
}

void InputManager::framebufferSizeCallback(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
}

void InputManager::onCursorPosition(double x, double y) {
    if (!cursorCaptured_) {
        return;
    }
    if (firstMouseSample_) {
        lastMousePosition_ = {x, y};
        firstMouseSample_ = false;
        return;
    }

    mouseDelta_.x += x - lastMousePosition_.x;
    mouseDelta_.y += lastMousePosition_.y - y;
    lastMousePosition_ = {x, y};
}

void InputManager::onScroll(double yOffset) {
    if (cursorCaptured_) {
        scrollDelta_ += yOffset;
    }
}
