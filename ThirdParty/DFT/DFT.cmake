option(WITH_DFT "Enable DFT Tracker (Requires OpenMP)" OFF)
if(WITH_DFT)
	add_subdirectory(${CMAKE_CURRENT_LIST_DIR})
	set(THIRD_PARTY_TRACKERS ${THIRD_PARTY_TRACKERS} DFT/src/DFT)
	set(THIRD_PARTY_INCLUDE_DIRS ${THIRD_PARTY_INCLUDE_DIRS} DFT/include)
	set(THIRD_PARTY_LIBS ${THIRD_PARTY_LIBS} dft)
	set(THIRD_PARTY_RUNTIME_FLAGS ${THIRD_PARTY_RUNTIME_FLAGS} -fopenmp)
else(WITH_DFT)		
	set(THIRD_PARTY_DEFINITIONS ${THIRD_PARTY_DEFINITIONS} DISABLE_DFT)
	message(STATUS "DFT disabled")
endif(WITH_DFT)

