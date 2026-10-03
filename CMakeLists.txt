cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Balance)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    User/Src/LED.c
    User/Src/uart.c
    User/Src/imu.c
    User/Src/encoder.c
    User/Src/event.c
    User/Src/key.c
    User/Src/motor.c
    User/Src/Analog.c
    User/Src/Buzzer.c

    # TFT LCD（ST7735S，SPI1）
    User/Src/TFT/TFT.c
    User/Src/TFT/TFT_Draw.c
    User/Src/TFT/TFT_Bitmap.c
    User/Src/TFT/TFT_Font.c
    User/Src/TFT/TFT_Driver.c
    User/Src/TFT/TFT_App.c
    User/Src/TFT/TFT_Display.c
    User/Src/TFT/TFT_BootSplash.c
    User/Src/TFT/TFT_BootSplashData.c

    # KK_UI（单色菜单 UI 运行时，只依赖 TFT 公共接口）
    User/Src/UI/KK_UI.c
    User/Src/UI/KK_UI_Anim.c
    User/Src/UI/KK_UI_Dialog.c
    User/Src/UI/KK_UI_Draw.c
    User/Src/UI/KK_UI_Home.c
    User/Src/UI/KK_UI_Menu.c
    User/Src/UI/KK_UI_Nav.c
    User/Src/UI/KK_UI_Page.c
    User/Src/UI/KK_UI_FontZh16.c

    # KK_UI 应用层：页面拓扑、图标和输入归一化
    User/Src/UI/KK_UI_App.c
    User/Src/UI/KK_UI_AppIcons.c
    User/Src/UI/KK_UI_Input.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    User/Inc
    User/Inc/TFT
    User/Inc/UI
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
