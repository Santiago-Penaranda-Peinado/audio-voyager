cmake_minimum_required(VERSION 3.22)
project(audio_voyager VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Optimization and warning flags
if(MSVC)
    add_compile_options(/O2 /W4 /fp:fast /permissive- /utf-8)
else()
    add_compile_options(
        -O3
        -Wall
        -Wextra
        -Wpedantic
        -ffast-math
        -pthread
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_compile_options(-march=native)
    endif()
endif()

find_package(Threads REQUIRED)

# ------------------------------------------------------------------------------
# 1. GLFW3 (System / vcpkg / FetchContent)
# ------------------------------------------------------------------------------
if(NOT WIN32)
    find_package(glfw3 3.3 QUIET)
    if(NOT glfw3_FOUND)
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(GLFW QUIET glfw3)
        endif()
    endif()
endif()

if(NOT glfw3_FOUND AND NOT GLFW_FOUND)
    message(STATUS "[audio-voyager] Fetching GLFW via FetchContent...")
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# ------------------------------------------------------------------------------
# 2. GLM Mathematics
# ------------------------------------------------------------------------------
if(NOT WIN32)
    find_package(glm QUIET)
endif()

if(NOT glm_FOUND)
    if(NOT WIN32)
        find_path(GLM_INCLUDE_DIR "glm/glm.hpp" PATHS /usr/include /usr/local/include)
    endif()
    if(NOT GLM_INCLUDE_DIR)
        message(STATUS "[audio-voyager] Fetching GLM via FetchContent...")
        FetchContent_Declare(
            glm
            GIT_REPOSITORY https://github.com/g-truc/glm.git
            GIT_TAG 1.0.1
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(glm)
    endif()
endif()

# ------------------------------------------------------------------------------
# 3. Dear ImGui (v1.91.8)
# ------------------------------------------------------------------------------
message(STATUS "[audio-voyager] Fetching Dear ImGui via FetchContent...")
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include
    )
    if(TARGET glfw)
        target_link_libraries(imgui PUBLIC glfw)
    elseif(GLFW_LIBRARIES)
        target_link_libraries(imgui PUBLIC ${GLFW_LIBRARIES})
    endif()
    if(WIN32)
        target_link_libraries(imgui PUBLIC opengl32)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(imgui PUBLIC OpenGL::GL)
    endif()
endif()

# ------------------------------------------------------------------------------
# 4. Essentia C++ (Optional on Windows, Auto-detected on Linux/Docker)
# ------------------------------------------------------------------------------
set(HAS_ESSENTIA FALSE)
if(NOT WIN32)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(ESSENTIA QUIET essentia)
        if(ESSENTIA_FOUND)
            set(HAS_ESSENTIA TRUE)
            message(STATUS "[audio-voyager] Found Essentia C++ library: ${ESSENTIA_VERSION}")
        endif()
    endif()

    if(NOT HAS_ESSENTIA AND EXISTS "/usr/local/include/essentia/essentia.h")
        find_library(ESSENTIA_LIB NAMES essentia PATHS /usr/local/lib /usr/lib)
        if(ESSENTIA_LIB)
            set(HAS_ESSENTIA TRUE)
            set(ESSENTIA_LIBRARIES ${ESSENTIA_LIB})
            set(ESSENTIA_INCLUDE_DIRS /usr/local/include /usr/include/eigen3)
            message(STATUS "[audio-voyager] Found Essentia C++ manually at /usr/local")
        endif()
    endif()
endif()

# ------------------------------------------------------------------------------
# Sources & Executable
# ------------------------------------------------------------------------------
set(SOURCES
    src/main.cpp
    src/audio/miniaudio_impl.cpp
    src/audio/audio_capture.cpp
    src/audio/stream_a_raw.cpp
    src/audio/stream_b_physics.cpp
    src/engine/audio_engine.cpp
    src/brain/semantic_classifier_ml.cpp
    src/brain/semantic_brain.cpp
    src/brain/waterfall_analyzer.cpp
    src/director/autonomous_art_director.cpp
    src/graphics/gl_context.cpp
    src/graphics/fbo.cpp
    src/graphics/shader.cpp
    src/graphics/particle_system.cpp
    src/graphics/oscilloscope_renderer.cpp
    src/graphics/postprocess.cpp
    src/graphics/imgui_overlay.cpp
    src/graphics/renderer.cpp
    third_party/glad/src/glad.c
)

set(HEADERS
    src/core/types.hpp
    src/core/lockfree_ring_buffer.hpp
    src/core/triple_buffer.hpp
    src/audio/audio_capture.hpp
    src/audio/stream_a_raw.hpp
    src/audio/stream_b_physics.hpp
    src/engine/audio_engine.hpp
    src/brain/semantic_classifier_ml.hpp
    src/brain/semantic_brain.hpp
    src/brain/waterfall_analyzer.hpp
    src/director/autonomous_art_director.hpp
    src/graphics/gl_context.hpp
    src/graphics/fbo.hpp
    src/graphics/shader.hpp
    src/graphics/particle_system.hpp
    src/graphics/oscilloscope_renderer.hpp
    src/graphics/postprocess.hpp
    src/graphics/imgui_overlay.hpp
    src/graphics/renderer.hpp
)

add_executable(audio_voyager ${SOURCES} ${HEADERS})

target_include_directories(audio_voyager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include
)

if(TARGET glm::glm)
    target_link_libraries(audio_voyager PRIVATE glm::glm)
elseif(GLM_INCLUDE_DIR)
    target_include_directories(audio_voyager PRIVATE ${GLM_INCLUDE_DIR})
endif()

target_link_libraries(audio_voyager PRIVATE
    Threads::Threads
    imgui
)

if(TARGET glfw)
    target_link_libraries(audio_voyager PRIVATE glfw)
elseif(glfw3_FOUND)
    target_link_libraries(audio_voyager PRIVATE glfw)
elseif(GLFW_FOUND)
    target_link_libraries(audio_voyager PRIVATE ${GLFW_LIBRARIES})
    target_include_directories(audio_voyager PRIVATE ${GLFW_INCLUDE_DIRS})
endif()

if(HAS_ESSENTIA)
    target_compile_definitions(audio_voyager PRIVATE USE_ESSENTIA)
    target_include_directories(audio_voyager PRIVATE ${ESSENTIA_INCLUDE_DIRS})
    target_link_libraries(audio_voyager PRIVATE ${ESSENTIA_LIBRARIES})
    if(UNIX AND NOT APPLE)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(FFTW3 QUIET fftw3f fftw3)
        if(FFTW3_FOUND)
            target_link_libraries(audio_voyager PRIVATE ${FFTW3_LIBRARIES})
        endif()
    endif()
else()
    message(STATUS "[audio-voyager] Using Native C++20 DSP Engine.")
endif()

# Platform-specific audio / system libraries
if(WIN32)
    target_link_libraries(audio_voyager PRIVATE winmm ole32 opengl32 gdi32 user32 kernel32)
    if(MINGW)
        target_link_options(audio_voyager PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
elseif(APPLE)
    target_link_libraries(audio_voyager PRIVATE "-framework CoreAudio" "-framework AudioToolbox" "-framework CoreFoundation")
elseif(UNIX)
    find_package(OpenGL REQUIRED)
    target_link_libraries(audio_voyager PRIVATE OpenGL::GL m dl)
    find_library(ALSA_LIB asound)
    if(ALSA_LIB)
        target_link_libraries(audio_voyager PRIVATE ${ALSA_LIB})
    endif()
    find_library(PULSE_LIB pulse)
    if(PULSE_LIB)
        target_link_libraries(audio_voyager PRIVATE ${PULSE_LIB})
    endif()
endif()

set_target_properties(audio_voyager PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
