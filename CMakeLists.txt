
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
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
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
# BUILT-IN LANGUAGES
# ==========================================
# resources/locales/*.json are compiled into the
# executable so the built-in languages can never be
# missing or out of date at run time. Regenerated
# whenever a locale file changes.

file(GLOB LOCALE_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/locales/*.json
)

set(EMBEDDED_LOCALES_CPP ${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedLocales.cpp)

add_custom_command(
    OUTPUT ${EMBEDDED_LOCALES_CPP}
    COMMAND ${CMAKE_COMMAND}
        -DLOCALES_DIR=${CMAKE_CURRENT_SOURCE_DIR}/resources/locales
        -DOUTPUT=${EMBEDDED_LOCALES_CPP}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedLocales.cmake
    DEPENDS ${LOCALE_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedLocales.cmake
    COMMENT "Embedding locale files"
    VERBATIM
)

# ==========================================
# APPLICATION
# ==========================================

add_executable(BeeKeeper

    src/main.cpp
    src/App.cpp
    src/config/Theme.cpp
    src/database/DatabaseManager.cpp
    src/database/DatabaseLocation.cpp
    src/database/DatabaseMigrations.cpp
    src/repositories/ApiaryRepository.cpp
    src/repositories/HiveRepository.cpp
    src/screens/ApiaryScreen.cpp
    src/screens/HiveFormDialog.cpp
    src/screens/HiveDetailScreen.cpp
    src/screens/BoxDialog.cpp
    src/screens/InspectionScreen.cpp
    src/screens/Dashboard.cpp
    src/screens/MaintenanceUI.cpp
    src/screens/MiteDialogs.cpp
    src/repositories/InspectionRepository.cpp
    src/repositories/MiteRepository.cpp
    src/repositories/MaintenanceRepository.cpp
    src/settings/MaintenanceSchedule.cpp
    src/models/InspectionOptions.cpp
    src/localization/TimeFormat.cpp
    src/repositories/BoxRepository.cpp
    src/models/HiveEquipment.cpp
    src/screens/SettingsEditors.cpp
    src/screens/SettingsScreen.cpp
    src/screens/SetupWizard.cpp
    src/repositories/SettingsRepository.cpp
    src/settings/Seasons.cpp
    src/settings/SettingsDefaults.cpp
    src/settings/RegionalSettings.cpp
    src/config/FontManager.cpp
    src/config/Paths.cpp
    ${EMBEDDED_LOCALES_CPP}
    src/localization/Localization.cpp
    src/localization/DateFormat.cpp
    src/ui/Widgets.cpp
    src/ui/Display.cpp
    
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}

    ${SQLITE_DIR}/sqlite3.c
)

target_include_directories(BeeKeeper PRIVATE

    src
    external/sqlite

    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMGUI_DIR}/misc/cpp

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

# ==========================================
# RESOURCES NEXT TO THE EXECUTABLE
# ==========================================
# Keeps <build>/resources (fonts, locales) identical to
# the project's resources after every build, so starting
# BeeKeeper from the build folder never uses a stale copy.

add_custom_command(TARGET BeeKeeper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:BeeKeeper>/resources
    COMMENT "Copying resources next to the executable"
)
