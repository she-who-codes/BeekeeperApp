
cmake_minimum_required(VERSION 3.20)

project(BeeKeeper VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# GLFW
# ==========================================

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/glfw)

find_package(OpenGL REQUIRED)

# ==========================================
# IMGUI
# ==========================================

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# ==========================================
# IMPLOT
# ==========================================

set(IMPLOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/implot)

set(IMPLOT_SOURCES
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
)

# ==========================================
# SQLITE
# ==========================================

set(SQLITE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/sqlite)

# ==========================================
# APPLICATION
# ==========================================

add_executable(BeeKeeper

    src/main.cpp
    src/config/Theme.cpp
    src/database/DatabaseManager.cpp
    src/database/DatabaseMigrations.cpp
    src/repositories/ApiaryRepository.cpp
    src/repositories/HiveRepository.cpp
    src/screens/ApiaryScreen.cpp
    src/config/FontManager.cpp
    
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}

    ${SQLITE_DIR}/sqlite3.c
)

target_include_directories(BeeKeeper PRIVATE

    src
    external/sqlite

    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends

    ${IMPLOT_DIR}

    ${SQLITE_DIR}

    ${CMAKE_CURRENT_SOURCE_DIR}/external/json/include
)

target_compile_definitions(BeeKeeper PRIVATE
    GLFW_INCLUDE_NONE
)

target_link_libraries(BeeKeeper PRIVATE
    glfw
    OpenGL::GL
)
