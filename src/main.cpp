#define NOMINMAX
#include <windows.h>

#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>
#include <string>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include "config/AppConfig.h"
#include "config/Theme.h"
#include "config/FontManager.h"
#include "config/LayoutUI.h"

#include "database/DatabaseManager.h"
#include "database/DatabaseMigrations.h"

#include "App.h"

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
        std::cerr << "Failed to initialize GLFW."
                  << std::endl;
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

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

    // Set minimum window dimensions.
    glfwSetWindowSizeLimits(
        window,
        AppConfig::Window::MinimumWidth,
        AppConfig::Window::MinimumHeight,
        GLFW_DONT_CARE,
        GLFW_DONT_CARE
    );

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // ----------------------------------------------
    // 3. Detect Display DPI
    // ----------------------------------------------

    float dpiX = 1.0f;
    float dpiY = 1.0f;

    glfwGetWindowContentScale(window, &dpiX, &dpiY);

    const float dpiScale = std::max(dpiX, dpiY);

    std::cout << "Display DPI scale: "
              << dpiScale << std::endl;

    // ----------------------------------------------
    // 4. Initialize Dear ImGui
    // ----------------------------------------------

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    // ----------------------------------------------
    // 5. Initialize ImPlot
    // ----------------------------------------------

    ImPlot::CreateContext();

    // ----------------------------------------------
    // 6. Configure ImGui
    // ----------------------------------------------

    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // ----------------------------------------------
    // 7. Load BeeKeeper Fonts
    // ----------------------------------------------

    if (!Fonts::Load())
    {
        std::cerr << "Warning: Font initialization failed."
                  << std::endl;
    }

    // ----------------------------------------------
    // 8. Apply BeeKeeper Theme
    // ----------------------------------------------

    Theme::Apply();

    // Scale all ImGui dimensions for display DPI.
    ImGui::GetStyle().ScaleAllSizes(dpiScale);

    // Fonts are scaled globally by ImGui (1.92+); sizes in
    // LayoutUI.h stay in design pixels.
    ImGui::GetStyle().FontScaleDpi = dpiScale;

    // Fixed sizes from LayoutUI.h are scaled via UI::Px().
    UI::SetDpiScale(dpiScale);

    // ----------------------------------------------
    // 9. Initialize ImGui Backends
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
    // 10. Initialize SQLite Database
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
    // 11. Run Database Migrations
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
    // 12. Initialize Application
    // ----------------------------------------------

    // Owns repositories and screens, loads settings and
    // applies the keeper's language and date format.
    App app(database);
    std::string appError;

    if (!app.Initialize(appError))
    {
        std::cerr << "Application initialization failed: "
                  << appError
                  << std::endl;

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        ImPlot::DestroyContext();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();

        return 1;
    }

    if (!appError.empty())
    {
        std::cerr << "Warning: " << appError << std::endl;
    }

    std::cout << "BeeKeeper initialized successfully!"
              << std::endl;

    // ----------------------------------------------
    // 14. Main Application Loop
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

        app.Render();

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

        // Use centralized BeeKeeper background color.
        const ImVec4& background =
            Theme::Colors::Background;

        glClearColor(
            background.x,
            background.y,
            background.z,
            background.w
        );

        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(
            ImGui::GetDrawData()
        );

        glfwSwapBuffers(window);
    }

    // ----------------------------------------------
    // 15. Shutdown Application
    // ----------------------------------------------

    std::cout << "Shutting down BeeKeeper..."
              << std::endl;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    // Database closes automatically when its
    // DatabaseManager instance is destroyed.

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
