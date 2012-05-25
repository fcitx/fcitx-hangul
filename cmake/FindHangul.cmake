# - Try to find the Libpinyin libraries
# Once done this will define
#
#  HANGUL_FOUND - system has HANGUL
#  HANGUL_INCLUDE_DIR - the HANGUL include directory
#  HANGUL_LIBRARIES - HANGUL library
#
# Copyright (c) 2012 CSSlayer <wengxt@gmail.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if(HANGUL_INCLUDE_DIR AND HANGUL_LIBRARIES)
    # Already in cache, be silent
    set(HANGUL_FIND_QUIETLY TRUE)
endif(HANGUL_INCLUDE_DIR AND HANGUL_LIBRARIES)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_LIBHANGUL "libhangul >= 0.0.12")

find_path(HANGUL_MAIN_INCLUDE_DIR
          NAMES hangul.h
          HINTS ${PC_LIBHANGUL_INCLUDE_DIRS})

find_library(HANGUL_LIBRARIES
             NAMES hangul
             HINTS ${PC_LIBHANGUL_LIBDIR})

set(HANGUL_INCLUDE_DIR "${HANGUL_MAIN_INCLUDE_DIR}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Hangul DEFAULT_MSG 
                                  HANGUL_LIBRARIES
                                  HANGUL_MAIN_INCLUDE_DIR
                                  )

mark_as_advanced(HANGUL_INCLUDE_DIR HANGUL_LIBRARIES)
