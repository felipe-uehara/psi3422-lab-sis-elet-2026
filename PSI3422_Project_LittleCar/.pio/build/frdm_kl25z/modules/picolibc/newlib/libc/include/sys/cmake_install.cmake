# Install script for directory: C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/Zephyr-Kernel")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "C:/Users/felip/.platformio/packages/toolchain-gccarmnoneeabi@1.80201.181220/bin/arm-none-eabi-objdump.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/sys" TYPE FILE FILES
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/auxv.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/cdefs.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/config.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/custom_file.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/_default_fcntl.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/dirent.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/dir.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/errno.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/fcntl.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/features.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/file.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/iconvnls.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/_initfini.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/_intsup.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/_locale.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/lock.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/param.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/queue.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/resource.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/sched.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/select.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/_select.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/_sigset.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/stat.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/_stdint.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/string.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/syslimits.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/timeb.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/time.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/times.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/_timespec.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/timespec.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/_timeval.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/tree.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/_types.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/types.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/_tz_structs.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/unistd.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/utime.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/wait.h"
    "C:/Users/felip/.platformio/packages/framework-zephyr/_pio/modules/lib/picolibc/newlib/libc/include/sys/_wait.h"
    )
endif()

