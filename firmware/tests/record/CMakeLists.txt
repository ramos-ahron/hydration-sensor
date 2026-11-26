cmake_minimum_required(VERSION 3.20.0)

find_package(
    Zephyr
    REQUIRED
    HINTS
    $ENV{ZEPHYR_BASE}
)

project(
    hs_test_record
    LANGUAGES C
)

target_include_directories(
    app
    PRIVATE
    ../../src
)

target_sources(app PRIVATE src/main.c ../../src/storage/record.c)
