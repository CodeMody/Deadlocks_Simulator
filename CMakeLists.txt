cmake_minimum_required(VERSION 3.15)
project(Deadlocks_Simulator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# -------------------------------------------------------------
# Find GTK‑3 (GUI Library)
# -------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

# Add the include directories and compiler flags that pkg‑config reports
include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})

# -------------------------------------------------------------
# Source files – everything needed for the simulator + GUI
# -------------------------------------------------------------
set(SRC
        Src/main.c
        Src/core/state.c
        Src/core/event.c
        Src/core/scheduler.c
        Src/policy/detect_policy.c
        Src/policy/banker_policy.c
        Src/policy/holdwait_policy.c
        Src/gui/gui.c
)

add_executable(${PROJECT_NAME} ${SRC})

# Link the GTK‑3 libraries
target_link_libraries(${PROJECT_NAME} ${GTK3_LIBRARIES})