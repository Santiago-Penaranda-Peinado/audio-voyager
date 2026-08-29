# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/workspace/build_cross/_deps/glm-src"
  "/workspace/build_cross/_deps/glm-build"
  "/workspace/build_cross/_deps/glm-subbuild/glm-populate-prefix"
  "/workspace/build_cross/_deps/glm-subbuild/glm-populate-prefix/tmp"
  "/workspace/build_cross/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp"
  "/workspace/build_cross/_deps/glm-subbuild/glm-populate-prefix/src"
  "/workspace/build_cross/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/workspace/build_cross/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/workspace/build_cross/_deps/glm-subbuild/glm-populate-prefix/src/glm-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
