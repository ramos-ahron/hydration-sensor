cmake_minimum_required(VERSION 3.20.0)

find_package(
    Zephyr
    REQUIRED
    HINTS
    $ENV{ZEPHYR_BASE}
)

project(
    hydration_sensor
    LANGUAGES C
)

target_include_directories(
    app
    PRIVATE
    src
)

file(GLOB_RECURSE app_sources CONFIGURE_DEPENDS src/*.c)
target_sources(
    app
    PRIVATE
    ${app_sources}
)
