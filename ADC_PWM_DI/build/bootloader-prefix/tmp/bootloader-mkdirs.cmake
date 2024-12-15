# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/jcardenas05/esp/v5.3.1/esp-idf/components/bootloader/subproject")
  file(MAKE_DIRECTORY "/home/jcardenas05/esp/v5.3.1/esp-idf/components/bootloader/subproject")
endif()
file(MAKE_DIRECTORY
  "/run/media/jcardenas05/2AB20958B2092A41/Real_Time_Operating_Systems/ADC_PWM_DI/build/bootloader"
  "/run/media/jcardenas05/2AB20958B2092A41/Real_Time_Operating_Systems/ADC_PWM_DI/build/bootloader-prefix"
  "/run/media/jcardenas05/2AB20958B2092A41/Real_Time_Operating_Systems/ADC_PWM_DI/build/bootloader-prefix/tmp"
  "/run/media/jcardenas05/2AB20958B2092A41/Real_Time_Operating_Systems/ADC_PWM_DI/build/bootloader-prefix/src/bootloader-stamp"
  "/run/media/jcardenas05/2AB20958B2092A41/Real_Time_Operating_Systems/ADC_PWM_DI/build/bootloader-prefix/src"
  "/run/media/jcardenas05/2AB20958B2092A41/Real_Time_Operating_Systems/ADC_PWM_DI/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/run/media/jcardenas05/2AB20958B2092A41/Real_Time_Operating_Systems/ADC_PWM_DI/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/run/media/jcardenas05/2AB20958B2092A41/Real_Time_Operating_Systems/ADC_PWM_DI/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
