

message(STATUS "MPI_INCLUDE_PATH : ${MPI_INCLUDE_PATH}")
message(STATUS "ROOT_INCLUDE_PATH : ${ROOT_INCLUDE_DIR}")
message(STATUS "C++ compiler is : ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ compiler Flags are : ${CMAKE_CXX_FLAGS}")
message(STATUS "C++ compiler identification is : ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "EPOCH     PATH is : ${EPOCH_PATH}")
message(STATUS "EPOCH SDF PATH is : ${SDF_PATH}")



include_directories(include
        ${MPI_INCLUDE_PATH}  
        ${ROOT_INCLUDE_DIR}
	${SDF_PATH}/include
	${EPOCH_PATH}/epoch2d/obj
	${CMAKE_SOURCE_DIR}/include
	}
)

link_directories(${OTHER_LINK_DIR} ${SDF_PATH}/lib ${EPOCH_PATH}/epoch2d/obj/)

set( SDFROOT2D_HEADERS
        src/TClientInfo2.h
        src/TMPIFile2.h
	src/cxxopts.hpp
)

set( SDFROOT2D_SRCS
        src/TClientInfo2.cxx
        src/TMPIFile2.cxx
	src/sdf_interface.f90
	src/sdf_reader.f90
)

ROOT_GENERATE_DICTIONARY( SDFROOT2DDict ${SDFROOT2D_HEADERS} LINKDEF src/Linkdef.h )
set( SDFROOT2D_DICTIONARIES SDFROOT2DDict.cxx)
add_library( SDFROOT2D SHARED ${SDFROOT2D_SRCS} ${SDFROOT2D_DICTIONARIES} )

target_link_libraries(SDFROOT2D
   PRIVATE
   MPI::MPI_C
   ${MPI_Fortran_LIBRARIES}
   ${MPI_LIBRARIES}
   ${ROOT_LIBRARIES}
   ${SDF_LIB}
   ${CMAKE_SOURCE_DIR}/sdf2root_2d/epoch_libs/epoch2d_lib.a	   
   )

set_target_properties(SDFROOT2D PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(SDFROOT2D PROPERTIES PUBLIC_HEADER "\
src/Linkdef.h;\
src/TClientInfo2.h;\
src/TMPIFile2.h;")

# Add iotests 
add_subdirectory(converter)


install(
        TARGETS SDFROOT2D
        LIBRARY DESTINATION lib
	PUBLIC_HEADER DESTINATION include
)

#Install Cling pcm file
install(
        FILES
        ${CMAKE_CURRENT_BINARY_DIR}/SDFROOT2DDict_rdict.pcm
        DESTINATION lib
)


