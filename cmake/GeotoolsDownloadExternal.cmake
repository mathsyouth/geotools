################################################################################

include(DownloadProject)

# With CMake 3.8 and above, we can hide warnings about git being in a
# detached head by passing an extra GIT_CONFIG option
set(GEOTOOLS_EXTRA_OPTIONS TLS_VERIFY OFF)
if(NOT (${CMAKE_VERSION} VERSION_LESS "3.8.0"))
	list(APPEND GEOTOOLS_EXTRA_OPTIONS GIT_CONFIG advice.detachedHead=false)
endif()

option(GEOTOOLS_SKIP_DOWNLOAD "Skip downloading external libraries" OFF)

# Shortcut functions
function(geotools_download_project name)
	if(NOT GEOTOOLS_SKIP_DOWNLOAD)
		download_project(
			PROJ         ${name}
			SOURCE_DIR   "${GEOTOOLS_EXTERNAL}/${name}"
			DOWNLOAD_DIR "${GEOTOOLS_EXTERNAL}/.cache/${name}"
			QUIET
			${GEOTOOLS_EXTRA_OPTIONS}
			${ARGN}
		)
	endif()
endfunction()

################################################################################

## Eigen
function(geotools_download_eigen)
	geotools_download_project(eigen
		GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
		GIT_TAG       3.3.9
	)
endfunction()

## geogram
function(geotools_download_geogram)
	geotools_download_project(geogram
		GIT_REPOSITORY https://github.com/BrunoLevy/geogram.git
		GIT_TAG        v1.8.4
	)
endfunction()

# Boost.Compute
function(geotools_download_compute)
	geotools_download_project(compute
		GIT_REPOSITORY https://github.com/boostorg/compute.git
		GIT_TAG        9189a761b79fcd4be2f38158b9cad164bac22fa2
	)
endfunction()
