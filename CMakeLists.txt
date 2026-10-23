cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if(NOT FIRMWARE_ROLE MATCHES "^(sender|receiver)$")
    message(FATAL_ERROR "Select -DFIRMWARE_ROLE=sender or -DFIRMWARE_ROLE=receiver")
endif()
# Preserve the selection during ESP-IDF's component dependency discovery pass.
idf_build_set_property(FIRMWARE_ROLE "${FIRMWARE_ROLE}")
project(hand_controlled_car)
