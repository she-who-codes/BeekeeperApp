
//#define GLFW_INCLUDE_NONE

#include <windows.h>
#include <GL/gl.h>

#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include "config/AppConfig.h"
#include "config/Theme.h"

#include "database/DatabaseManager.h"
#include "database/DatabaseMigrations.h"

#include "repositories/ApiaryRepository.h"
#include "repositories/HiveRepository.h"

#include "screens/ApiaryScreen.h"

#include <iostream>

// --------------------------------------------------
// GLFW Error Callback
// --------------------------------------------------

static void GLFWErrorCallback(int error, const char* description)
{
    std::cerr << "GLFW Error " << error
              << ": " << description << std::endl;
}

// --------------------------------------------------
// Main Application
// --------------------------------------------------

int main()
{
    // ----------------------------------------------
    // 1. Initialize GLFW
    // ----------------------------------------------

    glfwSetErrorCallback(GLFWErrorCallback);

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW." << std::endl;
        return 1;
    }

    // OpenGL configuration
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);

    // ----------------------------------------------
    // 2. Create Application Window
    // ----------------------------------------------

    GLFWwindow* window = glfwCreateWindow(
        AppConfig::Window::DefaultWidth,
        AppConfig::Window::DefaultHeight,
        "BeeKeeper",
        nullptr,
        nullptr
    );

    if (!window)
    {
        std::cerr << "Failed to create application window."
                  << std::endl;

        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    // Enable vertical synchronization.
    glfwSwapInterval(1);

    // ----------------------------------------------
    // 3. Initialize Dear ImGui
    // ----------------------------------------------

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    // ----------------------------------------------
    // 4. Initialize ImPlot
    // ----------------------------------------------

    ImPlot::CreateContext();

    // ----------------------------------------------
    // 5. Configure ImGui
    // ----------------------------------------------

    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Apply BeeKeeper's centralized theme.
    Theme::Apply();

    // ----------------------------------------------
    // 6. Initialize ImGui Backends
    // ----------------------------------------------

    const char* glslVersion = "#version 130";

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true))
    {
        std::cerr << "Failed to initialize ImGui GLFW backend."
                  << std::endl;

        ImPlot::DestroyContext();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();

        return 1;
    }

    if (!ImGui_ImplOpenGL3_Init(glslVersion))
    {
        std::cerr << "Failed to initialize ImGui OpenGL backend."
                  << std::endl;

        ImGui_ImplGlfw_Shutdown();

        ImPlot::DestroyContext();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();

        return 1;
    }

    // ----------------------------------------------
    // 7. Initialize SQLite Database
    // ----------------------------------------------

    DatabaseManager database;

    if (!database.Open("beekeeper.db"))
    {
        std::cerr << "Database connection failed: "
                  << database.GetLastError()
                  << std::endl;

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        ImPlot::DestroyContext();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();

        return 1;
    }

    std::cout << "Database connected successfully!"
              << std::endl;

    // ----------------------------------------------
    // 8. Run Database Migrations
    // ----------------------------------------------

    if (!DatabaseMigrations::Run(database))
    {
        std::cerr << "Database migration failed."
                  << std::endl;

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        ImPlot::DestroyContext();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();

        return 1;
    }

    // ----------------------------------------------
    // 9. Initialize Repositories
    // ----------------------------------------------

    ApiaryRepository apiaryRepository(database);

    HiveRepository hiveRepository(database);

    // ----------------------------------------------
    // 10. Initialize Application Screens
    // ----------------------------------------------

    ApiaryScreen apiaryScreen(
        apiaryRepository,
        hiveRepository
    );

    std::cout << "BeeKeeper initialized successfully!"
              << std::endl;

    // ----------------------------------------------
    // 11. Main Application Loop
    // ----------------------------------------------

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Start new ImGui frame.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();

        // ------------------------------------------
        // Render Application Screens
        // ------------------------------------------

        apiaryScreen.Render();

        // ------------------------------------------
        // Render ImGui
        // ------------------------------------------

        ImGui::Render();

        int displayWidth = 0;
        int displayHeight = 0;

        glfwGetFramebufferSize(
            window,
            &displayWidth,
            &displayHeight
        );

        glViewport(
            0,
            0,
            displayWidth,
            displayHeight
        );

        // Clear screen using BeeKeeper background color.
        const ImVec4& background = Theme::Colors::Background;

        glClearColor(
            background.x,
            background.y,
            background.z,
            background.w
        );

        glClear(GL_COLOR_BUFFER_BIT);

        // Render ImGui draw data.
        ImGui_ImplOpenGL3_RenderDrawData(
            ImGui::GetDrawData()
        );

        glfwSwapBuffers(window);
    }

    // ----------------------------------------------
    // 12. Shutdown Application
    // ----------------------------------------------

    std::cout << "Shutting down BeeKeeper..."
              << std::endl;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    // Database connection closes automatically when
    // DatabaseManager is destroyed.

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
