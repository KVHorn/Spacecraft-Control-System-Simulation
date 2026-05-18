cmake_minimum_required(VERSION 3.16)
project(SolarSystemSim CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include(FetchContent)

# ---- GLFW ----
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# ---- GLM ----
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1)
FetchContent_MakeAvailable(glm)

# ---- GLAD ----
FetchContent_Declare(glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.6
    SOURCE_SUBDIR cmake)
FetchContent_MakeAvailable(glad)
glad_add_library(glad_gl_core_33 STATIC API gl:core=3.3)

# ---- stb_image ----
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master)
FetchContent_MakeAvailable(stb)

# ---- tinygltf (glTF 2.0 loader) ----
FetchContent_Declare(tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG v2.9.3)
# Disable tinygltf's own examples / loaders we don't want
set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_GL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_VALIDATOR_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_BUILDER_EXAMPLE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinygltf)

# ---- nlohmann/json (header-only JSON library) ----
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# ---- libcurl (HTTP client for JPL Horizons API) ----
FetchContent_Declare(curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG curl-8_10_1)
set(BUILD_CURL_EXE      OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS   OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS   ON  CACHE BOOL "" FORCE)
set(BUILD_TESTING       OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAP   ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAPS  ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_TELNET ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_DICT   ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_FILE   ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_TFTP   ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_RTSP   ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_POP3   ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_IMAP   ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_SMTP   ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_SMB    ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_GOPHER ON  CACHE BOOL "" FORCE)
set(CURL_DISABLE_MQTT   ON  CACHE BOOL "" FORCE)
set(CURL_ZLIB           OFF CACHE BOOL "" FORCE)
if(WIN32)
    # Windows: use built-in Schannel (no OpenSSL needed)
    set(CURL_USE_SCHANNEL ON  CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL  OFF CACHE BOOL "" FORCE)
endif()
FetchContent_MakeAvailable(curl)

# ---- Dear ImGui ----
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.6)
FetchContent_MakeAvailable(imgui)

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
)
target_link_libraries(imgui PUBLIC glfw)

# ---- Main executable ----
add_executable(SolarSystemSim
    src/main.cpp
    src/Shader.cpp
    src/Texture.cpp
    src/Mesh.cpp
    src/Camera.cpp
    src/SolarSystem.cpp
    src/Spacecraft.cpp
    src/Model.cpp
    src/HorizonsApi.cpp
    src/tinygltf_impl.cpp
    src/stb_image_impl.cpp
)

target_include_directories(SolarSystemSim PRIVATE
    src/
    ${stb_SOURCE_DIR}
    ${tinygltf_SOURCE_DIR}
)

target_link_libraries(SolarSystemSim PRIVATE
    glfw glm::glm glad_gl_core_33 imgui
    nlohmann_json::nlohmann_json
    CURL::libcurl
)

# Copy shaders/textures/models next to the actual .exe (handles MSVC Release/Debug subfolders)
add_custom_command(TARGET SolarSystemSim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:SolarSystemSim>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/textures
        $<TARGET_FILE_DIR:SolarSystemSim>/textures
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/models
        $<TARGET_FILE_DIR:SolarSystemSim>/models
    COMMENT "Copying shaders, textures, and models next to the executable"
)

if(MSVC)
    target_compile_options(SolarSystemSim PRIVATE /W3)
    # Hide the console window in Release for cleaner fullscreen UX
    set_target_properties(SolarSystemSim PROPERTIES
        WIN32_EXECUTABLE $<IF:$<CONFIG:Release>,TRUE,FALSE>
        LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    )
else()
    target_compile_options(SolarSystemSim PRIVATE -Wall)
endif()
