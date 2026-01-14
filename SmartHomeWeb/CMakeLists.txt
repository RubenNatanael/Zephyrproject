cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(WebSmartHome)

target_sources(app PRIVATE
    src/main.c
    src/Room.c
    src/Web.c
)

zephyr_linker_sources(SECTIONS sections-rom.ld)
zephyr_linker_section(
    NAME http_resource_desc_test_http_service
    KVMA RAM_REGION GROUP RODATA_REGION
)

# Define where the generated files will go
set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)

# Generate the gzipped header file
generate_inc_file_for_target(app
    src/static_web_resources/index.html
    ${gen_dir}/index.html.gz.inc
    --gzip
)

# Tell the compiler where to find the generated file
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${gen_dir}
)