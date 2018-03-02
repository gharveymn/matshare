CMAKE_MINIMUM_REQUIRED(VERSION 3.7)
PROJECT(matshare)

SET(MEX_FILE_NAME "matshare_${DEBUG_SWITCH}")

INCLUDE_DIRECTORIES(${MATLAB_INCLUDE_DIR})

SET(SOURCE_FILES
		${MATLAB_INCLUDE_DIR}/mex.h
		matshare_.c headers/matshare_.h
	   utils.c headers/utils.h init.c headers/init.h headers/msh_types.h)

IF(UXWIN)
	SET(SOURCE_FILES
			${MATLAB_INCLUDE_DIR}/mex.h
			matshare_.c headers/matshare_.h
			utils.c headers/utils.h init.c headers/init.h headers/msh_types.h
			extlib/mman-win32/sys/mman.h)
ENDIF(UXWIN)

ADD_LIBRARY(${MEX_FILE_NAME} SHARED ${SOURCE_FILES} ${CMAKE_SOURCE_DIR}/Matlabdef.def)

LINK_DIRECTORIES(${MEX_FILE_NAME})
TARGET_LINK_LIBRARIES(${MEX_FILE_NAME} ${MATLAB_MEX_LIBRARY} ${MATLAB_MX_LIBRARY})

INSTALL(TARGETS ${MEX_FILE_NAME} DESTINATION ${INSTALL_OUTPUT_PATH})
