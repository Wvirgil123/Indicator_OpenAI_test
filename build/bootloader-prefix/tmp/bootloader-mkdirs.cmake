# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/Embedded/Espressif/frameworks/esp-idf-v5.0.1/components/bootloader/subproject"
  "D:/YZP/Desktop/Indicator_openai/build/bootloader"
  "D:/YZP/Desktop/Indicator_openai/build/bootloader-prefix"
  "D:/YZP/Desktop/Indicator_openai/build/bootloader-prefix/tmp"
  "D:/YZP/Desktop/Indicator_openai/build/bootloader-prefix/src/bootloader-stamp"
  "D:/YZP/Desktop/Indicator_openai/build/bootloader-prefix/src"
  "D:/YZP/Desktop/Indicator_openai/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/YZP/Desktop/Indicator_openai/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/YZP/Desktop/Indicator_openai/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
