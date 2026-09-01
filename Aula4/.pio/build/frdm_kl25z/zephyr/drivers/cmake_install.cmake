# Install script for directory: C:/Users/felip/.platformio/packages/framework-zephyr/drivers

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
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/drivers/disk/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/drivers/firmware/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/drivers/interrupt_controller/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/drivers/misc/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/drivers/pcie/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/drivers/usb/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/drivers/usb_c/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/drivers/clock_control/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/drivers/console/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/drivers/gpio/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/drivers/pinctrl/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/drivers/serial/cmake_install.cmake")
  include("C:/Users/felip/Documents/Escola_Politecnica_da_USP/psi3422-lab-sis-elet-2026/Aula4/.pio/build/frdm_kl25z/zephyr/drivers/timer/cmake_install.cmake")

endif()

