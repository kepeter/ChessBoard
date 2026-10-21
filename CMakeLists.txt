cmake_minimum_required(VERSION 3.20)
project(ChessBoard VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Writes compile_commands.json so editors (VS Code, clangd) know the real include
# paths and defines; without this the IDE has to guess and loses IntelliSense on reopen.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_AUTOMOC ON)

# 6.2 is a floor, not a pin: any Qt6 >= 6.2 found on the system is accepted.
find_package(Qt6 6.2 COMPONENTS Widgets Multimedia REQUIRED)

file(GLOB_RECURSE CHESSBOARD_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER CHESSBOARD_SOURCES EXCLUDE REGEX "src/main\\.cpp$")

# AUTOMOC discovers Q_OBJECT classes by scanning #include chains from .cpp files.
# A header with Q_OBJECT but no matching .cpp (e.g. clickable_row.hpp) can be missed,
# so headers are also listed as sources directly to force AUTOMOC to see them.
file(GLOB_RECURSE CHESSBOARD_HEADERS CONFIGURE_DEPENDS src/*.hpp)

add_executable(ChessBoard src/main.cpp ${CHESSBOARD_SOURCES} ${CHESSBOARD_HEADERS})

target_include_directories(ChessBoard PRIVATE src)
target_link_libraries(ChessBoard PRIVATE Qt6::Widgets Qt6::Multimedia)
target_compile_definitions(ChessBoard PRIVATE CHESSBOARD_VERSION="${PROJECT_VERSION}")

set(CHESSBOARD_ASSETS_DIR "$<TARGET_FILE_DIR:ChessBoard>/assets")

# A custom target (not a POST_BUILD step on ChessBoard) so the copy re-runs
# whenever any file under assets/ changes, even if no C++ source changed and
# the ChessBoard target itself is considered up to date and skipped.
file(GLOB_RECURSE CHESSBOARD_ASSET_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/assets/*)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/assets.stamp
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        ${CHESSBOARD_ASSETS_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/assets.stamp
    DEPENDS ${CHESSBOARD_ASSET_FILES}
    COMMENT "Copying assets"
)
add_custom_target(ChessBoard_assets DEPENDS ${CMAKE_BINARY_DIR}/assets.stamp)
add_dependencies(ChessBoard ChessBoard_assets)

# Installation rules for the ChessBoard application and its assets
install(TARGETS ChessBoard RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets DESTINATION share/chessboard)
install(FILES packaging/chessboard.desktop DESTINATION share/applications)
install(FILES packaging/chessboard.png DESTINATION share/icons/hicolor/256x256/apps)
install(FILES packaging/chessboard.svg DESTINATION share/icons/hicolor/scalable/apps)

# Common metadata for the ChessBoard application used in CPack configuration
set(CHESS_NAME "ChessBoard")
set(CHESS_VENDOR "Kornfeld Eliyahu Peter")
set(CHESS_SUMMARY "A desktop chess app")
set(CHESS_URL "https://github.com/kepeter/ChessBoard")
set(CHESS_CONTACT "kepeter@gmail.com")
set(CHESS_DESCRIPTION "A desktop chess app for playing against a UCI engine (such as Stockfish) with a coach-style helper alongside: live analysis, move feedback and optional on-board hints.")

# CPack settings below control `cpack` packaging (installers/archives), not the build itself.
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/dist")
set(CPACK_STRIP_FILES ON)

# Global CPack configuration using the common metadata variables
set(CPACK_PACKAGE_NAME "${CHESS_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "${CHESS_VENDOR}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${CHESS_URL}")
set(CPACK_PACKAGE_CONTACT "${CHESS_CONTACT}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/dist")

# RPM specific configuration using the common metadata variables
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_NAME "${CHESS_NAME}")
set(CPACK_RPM_PACKAGE_SUMMARY "${CHESS_SUMMARY}")
set(CPACK_RPM_PACKAGE_DESCRIPTION "${CHESS_DESCRIPTION}")
set(CPACK_RPM_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RPM_PACKAGE_VENDOR "${CHESS_VENDOR}")
set(CPACK_RPM_PACKAGE_URL "${CHESS_URL}")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Games")
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_PACKAGE_RECOMMENDS "stockfish")

# DEB specific configuration using the common metadata variables
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_NAME "${CHESS_NAME}")
set(CPACK_DEBIAN_PACKAGE_SUMMARY "${CHESS_SUMMARY}")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${CHESS_DESCRIPTION}")
set(CPACK_DEBIAN_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_VENDOR "${CHESS_VENDOR}")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CHESS_URL}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CHESS_VENDOR} <${CHESS_CONTACT}>")
set(CPACK_DEBIAN_PACKAGE_LICENSE "MIT")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "stockfish")

set(CPACK_GENERATOR "RPM;DEB")
include(CPack)
