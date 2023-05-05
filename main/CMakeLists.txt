cmake_minimum_required(VERSION 3.5)

set(UI_DIR ./ui)
file(GLOB_RECURSE UI_SOURCES ${UI_DIR}/*.c)
# 显示 UI_DIR 变量
message(STATUS "UI_DIR: ${UI_DIR}")
set(MODEL_DIR ./model)
file(GLOB_RECURSE MODEL_SOURCES ${MODEL_DIR}/*.c)

set(VIEW_DIR ./view)
file(GLOB_RECURSE VIEW_SOURCES ${VIEW_DIR}/*.c)

set(CONTROLLER_DIR ./controller)
file(GLOB_RECURSE CONTROLLER_SOURCES ${CONTROLLER_DIR}/*.c)

set(UTIL_DIR ./util)
file(GLOB_RECURSE UTIL_SOURCES ${UTIL_DIR}/*.c)

idf_component_register(
    SRCS "main.c" "lv_port.c" ${UI_SOURCES} ${MODEL_SOURCES} ${VIEW_SOURCES} ${CONTROLLER_SOURCES} ${UTIL_SOURCES}
    INCLUDE_DIRS "."  ${UI_DIR} ${MODEL_DIR} ${VIEW_DIR} ${CONTROLLER_DIR} ${UTIL_DIR}
    EMBED_TXTFILES timeapi_cert.pem)

# idf_component_register(SRCS "blink_example_main.c"
#                        INCLUDE_DIRS ".")


# include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# set(EXTRA_COMPONENT_DIRS  components)

# add_compile_options(-fdiagnostics-color=always -w)

# project(terminal_indicator)