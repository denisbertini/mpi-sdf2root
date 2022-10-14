
#Bring the header directory into the project
include_directories(
        .
        ${MPI_INCLUDE_PATH}
        ${ROOT_INCLUDE_DIR}
        ${SDFROOT_SOURCE_DIR}/sdf2root_2d/src
)

add_executable(sdf2root_2d sdf2root_2d.cxx)
target_link_libraries(sdf2root_2d PRIVATE MPI::MPI_CXX ${ROOT_LIBRARIES} SDFROOT2D)

install(
        TARGETS
        sdf2root_2d
        DESTINATION bin
)

