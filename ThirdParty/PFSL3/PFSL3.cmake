option(WITH_PFSL3 "Enable Geometric Particle Filter Tracker over SL3" OFF)
# set(CLAPACK_INCLUDE_DIR /usr/include/clapack CACHE PATH "Directory where CLAPACK headers: lapack.h and f2c.h are present")
if(WITH_PFSL3)
	find_package(CLAPACK)
	find_package(CLAPACK_HEADER QUIET)
	find_package(CBLAS)
	if(LAPACK_FOUND AND BLAS_FOUND)
		message(STATUS "LAPACK_LIBRARIES: ${LAPACK_LIBRARIES}")
		message(STATUS "CLAPACK_INCLUDE_DIR: ${CLAPACK_INCLUDE_DIR}")
		# message(STATUS "LAPACK_DEFINITIONS: ${LAPACK_DEFINITIONS}")
		# message(STATUS "LAPACK_LINKER_FLAGS: ${LAPACK_LINKER_FLAGS}")
		message(STATUS "BLAS_LIBRARIES: ${BLAS_LIBRARIES}")
		# message(STATUS "BLAS_INCLUDE_DIR: ${BLAS_INCLUDE_DIR}")
		# message(STATUS "BLAS_DEFINITIONS: ${BLAS_DEFINITIONS}")
		# message(STATUS "BLAS_LINKER_FLAGS: ${BLAS_LINKER_FLAGS}")
		set(THIRD_PARTY_TRACKERS ${THIRD_PARTY_TRACKERS} PFSL3/src/PFSL3)
		set(THIRD_PARTY_INCLUDE_DIRS ${THIRD_PARTY_INCLUDE_DIRS} PFSL3/include)
		# add_library (pfsl3 STATIC PFSL3/PFSL3.cc PFSL3/PFSL3.h)
		# target_include_directories(pfsl3 PUBLIC /usr/include/clapack)
		# install(TARGETS pfsl3 ARCHIVE DESTINATION MTF_LIB_INSTALL_DIR COMPONENT pfsl3)
		set(THIRD_PARTY_EXT_INCLUDE_DIRS ${THIRD_PARTY_EXT_INCLUDE_DIRS} ${CLAPACK_INCLUDE_DIR})
		set(THIRD_PARTY_LIBS ${THIRD_PARTY_LIBS} ${LAPACK_LIBRARIES} ${BLAS_LIBRARIES} f2c)
	else()
		message(STATUS "CLAPACK and/or BLAS not found so PFSL3 cannot be enabled")
		set(THIRD_PARTY_DEFINITIONS ${THIRD_PARTY_DEFINITIONS} DISABLE_PFSL3)
	endif()
else(WITH_PFSL3)	
	set(THIRD_PARTY_DEFINITIONS ${THIRD_PARTY_DEFINITIONS} DISABLE_PFSL3)
	message(STATUS "PFSL3 disabled")		
endif(WITH_PFSL3)



