# - this module looks for Matlab
# Defines:
#  MATLAB_INCLUDE_DIR: include path for mex.h
#  MATLAB_LIBRARIES:   required libraries: libmex, libmx
#  MATLAB_MEX_LIBRARY: path to libmex
#  MATLAB_MX_LIBRARY:  path to libmx

SET(MATLAB_FOUND 0)
IF("${MATLAB_ROOT}" STREQUAL "")
	MESSAGE(STATUS "MATLAB_ROOT environment variable not set.")
	MESSAGE(STATUS "In Linux this can be done in your user .bashrc file by appending the corresponding line, e.g:")
	MESSAGE(STATUS "export MATLAB_ROOT=/usr/local/MATLAB/R2012b")
	MESSAGE(STATUS "In Windows this can be done by adding system variable, e.g:")
	MESSAGE(STATUS "MATLAB_ROOT=D:\\Program Files\\MATLAB\\R2011a")
ELSE("${MATLAB_ROOT}" STREQUAL "")

	MESSAGE (STATUS "Searching for MATLAB at MATLAB_ROOT: ${MATLAB_ROOT}")

	FIND_PATH(MATLAB_INCLUDE_DIR mex.h
			${MATLAB_ROOT}/extern/include)

	INCLUDE_DIRECTORIES(${MATLAB_INCLUDE_DIR})

	MESSAGE (STATUS "MATLAB include directories found at ${MATLAB_INCLUDE_DIR}")

	FIND_LIBRARY(MATLAB_MEX_LIBRARY
			NAMES libmex mex
			PATHS ${MATLAB_ROOT}/bin ${MATLAB_ROOT}/extern/lib
			PATH_SUFFIXES glnxa64 glnx86 win64/microsoft win32/microsoft win64/mingw64)

	MESSAGE (STATUS "MATLAB MEX libraries found at ${MATLAB_MEX_LIBRARY}")

	FIND_LIBRARY(MATLAB_MX_LIBRARY
			NAMES libmx mx
			PATHS ${MATLAB_ROOT}/bin ${MATLAB_ROOT}/extern/lib
			PATH_SUFFIXES glnxa64 glnx86 win64/microsoft win32/microsoft win64/mingw64)

	MESSAGE (STATUS "MATLAB MX libraries found at ${MATLAB_MX_LIBRARY}")


ENDIF("${MATLAB_ROOT}" STREQUAL "")

# This is common to UNIX and Win32:
SET(MATLAB_LIBRARIES
		${MATLAB_MEX_LIBRARY}
		${MATLAB_MX_LIBRARY}
		)

IF(MATLAB_INCLUDE_DIR AND MATLAB_LIBRARIES)
	SET(MATLAB_FOUND 1)
	MESSAGE(STATUS "Matlab libraries will be used")
ENDIF(MATLAB_INCLUDE_DIR AND MATLAB_LIBRARIES)

MARK_AS_ADVANCED(
		MATLAB_LIBRARIES
		MATLAB_MEX_LIBRARY
		MATLAB_MX_LIBRARY
		MATLAB_INCLUDE_DIR
		MATLAB_FOUND
		MATLAB_ROOT
)
