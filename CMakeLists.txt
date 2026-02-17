cmake_minimum_required(VERSION 3.20)
project(swvcs VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows only - SolidWorks is Windows exclusive
if(NOT WIN32)
    message(FATAL_ERROR "swvcs only supports Windows (SolidWorks requirement)")
endif()

# -------------------------------------------------------
# Source files
# -------------------------------------------------------
set(SOURCES
    src/main.cpp
    src/sw_connection.cpp
    src/repository.cpp
    src/commit_engine.cpp
    src/revert_engine.cpp
    src/utils.cpp
)

set(HEADERS
    include/sw_connection.h
    include/repository.h
    include/commit_engine.h
    include/revert_engine.h
    include/utils.h
    include/types.h
)

add_executable(swvcs ${SOURCES} ${HEADERS})

target_include_directories(swvcs PRIVATE include)

# -------------------------------------------------------
# Windows / COM libraries
# -------------------------------------------------------
target_link_libraries(swvcs PRIVATE
    ole32       # COM init
    oleaut32    # OLE automation (IDispatch)
    uuid        # GUID definitions
)

# -------------------------------------------------------
# Compiler flags
# -------------------------------------------------------
target_compile_definitions(swvcs PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

# -------------------------------------------------------
# Third-party dependencies (fetched automatically)
# -------------------------------------------------------
include(FetchContent)

# nlohmann/json — commit metadata serialisation (header-only)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)

# SQLiteCpp — C++ SQLite wrapper (bundles its own sqlite3, no system install needed)
# Used for the local commit database (.swvcs/swvcs.db)
FetchContent_Declare(
    SQLiteCpp
    GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
    GIT_TAG        3.3.2
)
set(SQLITECPP_RUN_CPPCHECK     OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPLINT      OFF CACHE BOOL "" FORCE)
set(SQLITECPP_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
set(SQLITECPP_BUILD_EXAMPLES   OFF CACHE BOOL "" FORCE)
set(SQLITECPP_INTERNAL_SQLITE  ON  CACHE BOOL "" FORCE)  # compile sqlite3 into the static lib (no DLL needed at runtime)

FetchContent_MakeAvailable(nlohmann_json SQLiteCpp)

target_link_libraries(swvcs PRIVATE
    nlohmann_json::nlohmann_json
    SQLiteCpp
)

# -------------------------------------------------------
# Output to bin/
# -------------------------------------------------------
set_target_properties(swvcs PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

# -------------------------------------------------------
# GUI target (optional — only built when Qt6 is installed)
# Install Qt6 from https://www.qt.io/download-open-source
# then reconfigure cmake.
# -------------------------------------------------------
find_package(Qt6 QUIET COMPONENTS Widgets)

if (Qt6_FOUND)
    message(STATUS "Qt6 found at ${Qt6_DIR} — building swvcs-gui")

    set(GUI_SOURCES
        src/gui/main_gui.cpp
        src/gui/main_window.cpp
        src/gui/commit_dialog.cpp
        # GUI headers listed explicitly so AUTOMOC processes Q_OBJECT classes
        include/main_window.h
        include/commit_dialog.h
        # Shared backend (everything except the CLI main)
        src/sw_connection.cpp
        src/repository.cpp
        src/commit_engine.cpp
        src/revert_engine.cpp
        src/utils.cpp
    )

    add_executable(swvcs-gui WIN32 ${GUI_SOURCES})

    # AUTOMOC/AUTORCC per-target so the CLI target is unaffected
    set_target_properties(swvcs-gui PROPERTIES
        AUTOMOC ON
        AUTORCC ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    )

    target_include_directories(swvcs-gui PRIVATE include)

    target_link_libraries(swvcs-gui PRIVATE
        Qt6::Widgets
        ole32
        oleaut32
        uuid
        nlohmann_json::nlohmann_json
        SQLiteCpp
    )

    target_compile_definitions(swvcs-gui PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
    )
else()
    message(STATUS "Qt6 not found — skipping swvcs-gui (install Qt6 to build the GUI)")
endif()