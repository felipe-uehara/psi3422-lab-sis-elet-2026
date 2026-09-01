# Install script for directory: C:/Users/felip/.platformio/packages/framework-zephyr

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

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/arch/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/lib/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/soc/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/boards/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/subsys/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/drivers/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/_bare_module/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/acpica/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/cmsis/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/cmsis-dsp/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/cmsis-nn/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/cmsis_6/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/fatfs/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/hal_nxp/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/hal_st/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/hostap/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/liblc3/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/libmctp/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/libmetal/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/littlefs/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/loramac-node/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/lvgl/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/mbedtls/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/mcuboot/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/mipi-sys-t/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/nrf_wifi/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/open-amp/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/openthread/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/percepio/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/picolibc/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/segger/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/tinycrypt/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/uoscore-uedhoc/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/modules/zcbor/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/kernel/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/cmake/flash/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/cmake/usage/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/cmake/reports/cmake_install.cmake")

endif()

