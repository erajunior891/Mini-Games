# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Projects/Mini Games/WorldBorder/../endstone-main/endstone-main/include")
  file(MAKE_DIRECTORY "C:/Projects/Mini Games/WorldBorder/../endstone-main/endstone-main/include")
endif()
file(MAKE_DIRECTORY
  "C:/Projects/Mini Games/WorldBorder/build/_deps/endstone-build"
  "C:/Projects/Mini Games/WorldBorder/build/_deps/endstone-subbuild/endstone-populate-prefix"
  "C:/Projects/Mini Games/WorldBorder/build/_deps/endstone-subbuild/endstone-populate-prefix/tmp"
  "C:/Projects/Mini Games/WorldBorder/build/_deps/endstone-subbuild/endstone-populate-prefix/src/endstone-populate-stamp"
  "C:/Projects/Mini Games/WorldBorder/build/_deps/endstone-subbuild/endstone-populate-prefix/src"
  "C:/Projects/Mini Games/WorldBorder/build/_deps/endstone-subbuild/endstone-populate-prefix/src/endstone-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Projects/Mini Games/WorldBorder/build/_deps/endstone-subbuild/endstone-populate-prefix/src/endstone-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Projects/Mini Games/WorldBorder/build/_deps/endstone-subbuild/endstone-populate-prefix/src/endstone-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
