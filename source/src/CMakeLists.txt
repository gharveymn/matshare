CMAKE_MINIMUM_REQUIRED(VERSION 3.5)
PROJECT(matshare C)

SET(MEX_FILE_NAME "matshare_${DEBUG_SWITCH}")

INCLUDE_DIRECTORIES(${MATLAB_INCLUDE_DIR} headers)

SET(SOURCE_FILES
		${MATLAB_INCLUDE_DIR}/mex.h
		matshare_.c
		headers/matshare_.h
		mlerrorutils.c
		headers/mlerrorutils.h
		mshinit.c
		headers/mshinit.h
		mshutils.c
		headers/mshutils.h
		headers/mshtypes.h
		headers/mshexterntypes.h
		mshvariables.c
		headers/mshvariables.h
		headers/mshheader.h
		headers/opaque/mshheader.c
		headers/mshbasictypes.h
		headers/opaque/mshexterntypes.c
		mshsegments.c
		headers/mshsegments.h
		headers/opaque/mshtable.c
		headers/opaque/mshvariablenode.c
		headers/mshvariablenode.h
		headers/opaque/mshsegmentnode.c
		headers/mshsegmentnode.h
		mshlockfree.c
		headers/mshlockfree.h)

SET_SOURCE_FILES_PROPERTIES(${SOURCE_FILES} PROPERTIES LANGUAGE C)

ADD_LIBRARY(${MEX_FILE_NAME} SHARED ${SOURCE_FILES} ${CMAKE_SOURCE_DIR}/Matlabdef.def)

LINK_DIRECTORIES(${MEX_FILE_NAME})
TARGET_LINK_LIBRARIES(${MEX_FILE_NAME} ${MATLAB_MEX_LIBRARY} ${MATLAB_MX_LIBRARY})

INSTALL(TARGETS ${MEX_FILE_NAME} DESTINATION ${INSTALL_OUTPUT_PATH})
