#include "Player.h"
#include "Renderer.h"
#include "World.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
struct GameState {
    Player* player = nullptr;
    int* selectedSlot = nullptr;
    bool mouseCaptured = true;
    bool firstMouseSample = true;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
};

void framebufferSizeCallback(GLFWwindow*, int width, int height) { glViewport(0, 0, width, height); }

void mouseCallback(GLFWwindow* window, double x, double y) {
    auto* state = static_cast<GameState*>(glfwGetWindowUserPointer(window));
    if (!state || !state->mouseCaptured || !state->player) return;
    if (state->firstMouseSample) {
        state->lastMouseX = x; state->lastMouseY = y; state->firstMouseSample = false; return;
    }
    state->player->addMouseMovement(x - state->lastMouseX, state->lastMouseY - y);
    state->lastMouseX = x; state->lastMouseY = y;
}

void scrollCallback(GLFWwindow* window, double, double yOffset) {
    auto* state = static_cast<GameState*>(glfwGetWindowUserPointer(window));
    if (!state || !state->selectedSlot || yOffset == 0.0) return;
    const int direction = yOffset > 0.0 ? -1 : 1;
    *state->selectedSlot = (*state->selectedSlot + direction + 7) % 7;
}

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (key != GLFW_KEY_ESCAPE || action != GLFW_PRESS) return;
    auto* state = static_cast<GameState*>(glfwGetWindowUserPointer(window));
    if (!state) return;
    state->mouseCaptured = !state->mouseCaptured;
    state->firstMouseSample = true;
    glfwSetInputMode(window, GLFW_CURSOR, state->mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

int floorChunk(float coordinate) { return static_cast<int>(std::floor(coordinate / static_cast<float>(CHUNK_SIZE))); }

void printControls() {
    std::cout << "Voxel Frontier controls:\n"
              << "  WASD / Mouse   move and look\n"
              << "  Space          jump\n"
              << "  LMB / RMB      break / place selected block\n"
              << "  1-7 or wheel   select hotbar block\n"
              << "  F3             debug overlay\n"
              << "  Escape         release/capture mouse\n" << std::flush;
}
}

int main() {
    if (!glfwInit()) { std::cerr << "GLFW initialization failed.\n"; return EXIT_FAILURE; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Voxel Frontier: Living World", nullptr, nullptr);
    if (!window) { glfwTerminate(); return EXIT_FAILURE; }
    glfwMakeContextCurrent(window); glfwSwapInterval(1);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        glfwDestroyWindow(window); glfwTerminate(); return EXIT_FAILURE;
    }
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW);

    int result = EXIT_SUCCESS;
    try {
        Renderer renderer;
        World world(20260917U);
        std::cout << "Preparing spawn chunk..." << std::endl;
        world.generate(7);
        Player player(glm::vec3(0.5f, static_cast<float>(world.terrainHeight(0, 0) + 2), 0.5f));

        int selectedSlot = 0;
        constexpr std::array<Block, 7> HotbarBlocks{{Block::Grass, Block::Dirt, Block::Stone,
            Block::Sand, Block::Log, Block::Leaves, Block::Water}};
        GameState state{&player, &selectedSlot};
        glfwSetWindowUserPointer(window, &state);
        glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
        glfwSetCursorPosCallback(window, mouseCallback);
        glfwSetScrollCallback(window, scrollCallback);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        printControls();

        double previousTime = glfwGetTime();
        double fpsTimer = previousTime;
        int frameCounter = 0;
        float displayedFps = 0.0f;
        float worldTime = 35.0f;
        bool leftWasDown = false, rightWasDown = false, f3WasDown = false;
        bool showDebug = false;

        while (!glfwWindowShouldClose(window)) {
            const double now = glfwGetTime();
            const float deltaTime = std::min(static_cast<float>(now - previousTime), 0.05f);
            previousTime = now; worldTime += deltaTime; ++frameCounter;
            if (now - fpsTimer >= 0.5) {
                displayedFps = static_cast<float>(frameCounter / (now - fpsTimer));
                frameCounter = 0; fpsTimer = now;
            }
            glfwPollEvents();

            for (int i = 0; i < 7; ++i) if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS) selectedSlot = i;
            const bool f3Down = glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS;
            if (f3Down && !f3WasDown) showDebug = !showDebug;
            f3WasDown = f3Down;

            world.updateStreaming(player.cameraPosition(), 2);
            if (state.mouseCaptured) {
                player.update(deltaTime, window, world);
                const bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
                const bool rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
                RayHit hit;
                if (leftDown && !leftWasDown && world.raycast(player.cameraPosition(), player.lookDirection(), 6.0f, hit)) {
                    const Block broken = world.getBlock(hit.block.x, hit.block.y, hit.block.z);
                    world.setBlock(hit.block.x, hit.block.y, hit.block.z, Block::Air);
                    renderer.spawnBreakParticles(hit.block, broken);
                }
                if (rightDown && !rightWasDown && world.raycast(player.cameraPosition(), player.lookDirection(), 6.0f, hit)) {
                    const Block selected = HotbarBlocks[static_cast<std::size_t>(selectedSlot)];
                    const bool obstructsPlayer = isSolid(selected) && world.blockIntersectsAabb(hit.adjacent, player.aabbMinimum(), player.aabbMaximum());
                    if (world.getBlock(hit.adjacent.x, hit.adjacent.y, hit.adjacent.z) == Block::Air && !obstructsPlayer)
                        world.setBlock(hit.adjacent.x, hit.adjacent.y, hit.adjacent.z, selected);
                }
                leftWasDown = leftDown; rightWasDown = rightDown;
            } else { leftWasDown = false; rightWasDown = false; }
            renderer.updateParticles(deltaTime);

            int width = 0, height = 0; glfwGetFramebufferSize(window, &width, &height);
            if (width <= 0 || height <= 0) continue;
            glViewport(0, 0, width, height); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            const glm::mat4 projection = glm::perspective(glm::radians(74.0f), static_cast<float>(width) / height, 0.08f, 230.0f);
            const glm::mat4 view = player.viewMatrix();
            renderer.renderSky(view, projection, worldTime);
            renderer.renderWorld(world, view, projection, player.cameraPosition(), worldTime);
            renderer.renderParticles(view, projection);

            std::string debugText;
            if (showDebug) {
                const glm::vec3 position = player.cameraPosition() - glm::vec3(0.0f, Player::EyeHeight, 0.0f);
                std::ostringstream debug;
                debug << std::fixed << std::setprecision(1) << "FPS " << displayedFps << '\n'
                      << "XYZ " << position.x << " / " << position.y << " / " << position.z << '\n'
                      << "CHUNK " << floorChunk(position.x) << " / " << floorChunk(position.z) << '\n'
                      << "BIOME " << world.biomeNameAt(static_cast<int>(std::floor(position.x)), static_cast<int>(std::floor(position.z))) << '\n'
                      << "CHUNKS " << world.loadedChunkCount() << " + " << world.pendingChunkCount() << '\n'
                      << "RENDER " << world.renderDistance();
                debugText = debug.str();
            }
            renderer.renderHud(width, height, selectedSlot, debugText);
            glfwSwapBuffers(window);
        }
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n'; result = EXIT_FAILURE;
    }
    glfwDestroyWindow(window); glfwTerminate(); return result;
}

