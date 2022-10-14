# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# determine the compiler to use for Fortran programs
# NOTE, a generator may set CMAKE_Fortran_COMPILER before
# loading this file to force a compiler.
# use environment variable FC first if defined by user, next use
# the cmake variable CMAKE_GENERATOR_FC which can be defined by a generator
# as a default compiler

include(${CMAKE_ROOT}/Modules/CMakeDetermineCompiler.cmake)
include(Platform/${CMAKE_SYSTEM_NAME}-Determine-Fortran OPTIONAL)
include(Platform/${CMAKE_SYSTEM_NAME}-Fortran OPTIONAL)
if(NOT CMAKE_Fortran_COMPILER_NAMES)
  set(CMAKE_Fortran_COMPILER_NAMES f95)
endif()

if(${CMAKE_GENERATOR} MATCHES "Visual Studio")
elseif("${CMAKE_GENERATOR}" MATCHES "Xcode")
  set(CMAKE_Fortran_COMPILER_XCODE_TYPE sourcecode.fortran.f90)
  _cmake_find_compiler_path(Fortran)
else()
  if(NOT CMAKE_Fortran_COMPILER)
    # prefer the environment variable CC
    if(NOT $ENV{FC} STREQUAL "")
      get_filename_component(CMAKE_Fortran_COMPILER_INIT $ENV{FC} PROGRAM PROGRAM_ARGS CMAKE_Fortran_FLAGS_ENV_INIT)
      if(CMAKE_Fortran_FLAGS_ENV_INIT)
        set(CMAKE_Fortran_COMPILER_ARG1 "${CMAKE_Fortran_FLAGS_ENV_INIT}" CACHE STRING "Arguments to Fortran compiler")
      endif()
      if(EXISTS ${CMAKE_Fortran_COMPILER_INIT})
      else()
        message(FATAL_ERROR "Could not find compiler set in environment variable FC:\n$ENV{FC}.")
      endif()
    endif()

    # next try prefer the compiler specified by the generator
    if(CMAKE_GENERATOR_FC)
      if(NOT CMAKE_Fortran_COMPILER_INIT)
        set(CMAKE_Fortran_COMPILER_INIT ${CMAKE_GENERATOR_FC})
      endif()
    endif()

    # finally list compilers to try
    if(NOT CMAKE_Fortran_COMPILER_INIT)
      # Known compilers:
      #  ftn: Cray fortran compiler wrapper
      #  gfortran: putative GNU Fortran 95+ compiler (in progress)
      #  frt: Fujitsu Fortran compiler
      #  pathf90/pathf95/pathf2003: PathScale Fortran compiler
      #  pgfortran: Portland Group Fortran compilers
      #  nvfortran: NVHPC Fotran compiler
      #  flang: Flang Fortran compiler
      #  xlf: IBM (AIX) Fortran compiler
      #  lf95: Lahey-Fujitsu F95 compiler
      #  fl32: Microsoft Fortran 77 "PowerStation" compiler
      #  af77: Apogee F77 compiler for Intergraph hardware running CLIX
      #  epcf90: "Edinburgh Portable Compiler" F90
      #  fort: Compaq (now HP) Fortran 90/95 compiler for Tru64 and Linux/Alpha
      #  ifx: Intel Fortran LLVM-based compiler
      #  ifort: Intel Classic Fortran compiler
      #  nagfor: NAG Fortran compiler
      #
      #  GNU is last to be searched,
      #  so if you paid for a compiler it is picked by default.
      set(CMAKE_Fortran_COMPILER_LIST
        ftn
        ifx ifort nvfortran pgfortran lf95 xlf fort
        flang lfortran frt nagfor
        gfortran
        )
