#[=======================================================================[.rst:
FindSampleRate
--------------

Find the SampleRate (libsamplerate) audio sample rate conversion library.

Imported Targets
^^^^^^^^^^^^^^^^

``SampleRate::samplerate``
  The samplerate library, if found.

Result Variables
^^^^^^^^^^^^^^^^

``SampleRate_FOUND``
  True if the library was found.
``SampleRate_INCLUDE_DIRS``
  Include directories needed to use samplerate.
``SampleRate_LIBRARIES``
  Libraries needed to link to samplerate.

Hints
^^^^^

``SAMPLERATE_ROOT``
  A directory prefix for the samplerate installation.
#]=======================================================================]

# Try pkg-config first
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_Samplerate QUIET samplerate)
endif()

# Extra search paths for macOS package managers
set(_samplerate_extra_paths
    /opt/homebrew
    /opt/local
)

find_path(SampleRate_INCLUDE_DIR
    NAMES samplerate.h
    HINTS
        ${SAMPLERATE_ROOT}
        ENV SAMPLERATE_ROOT
        ${PC_Samplerate_INCLUDEDIR}
        ${PC_Samplerate_INCLUDE_DIRS}
    PATH_SUFFIXES include
    PATHS ${_samplerate_extra_paths}
)

find_library(SampleRate_LIBRARY
    NAMES samplerate
    HINTS
        ${SAMPLERATE_ROOT}
        ENV SAMPLERATE_ROOT
        ${PC_Samplerate_LIBDIR}
        ${PC_Samplerate_LIBRARY_DIRS}
    PATH_SUFFIXES lib lib64
    PATHS ${_samplerate_extra_paths}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SampleRate
    REQUIRED_VARS SampleRate_LIBRARY SampleRate_INCLUDE_DIR)

if(SampleRate_FOUND)
    set(SampleRate_INCLUDE_DIRS "${SampleRate_INCLUDE_DIR}")
    set(SampleRate_LIBRARIES "${SampleRate_LIBRARY}")

    if(NOT TARGET SampleRate::samplerate)
        add_library(SampleRate::samplerate UNKNOWN IMPORTED)
        set_target_properties(SampleRate::samplerate PROPERTIES
            IMPORTED_LOCATION "${SampleRate_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SampleRate_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(SampleRate_INCLUDE_DIR SampleRate_LIBRARY)
