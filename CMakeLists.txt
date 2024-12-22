# Minimum version of CMake required
cmake_minimum_required(VERSION 3.10)

# Project name and language
project(Ghlble LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define sources
set(SOURCES
	main.cpp
	guitar.cpp
	gamepad.cpp
	ResettableTimer.cpp
)

# Add include directories for your project
include_directories(
	${CMAKE_SOURCE_DIR}/include
	/usr/include/glib-2.0
	/usr/lib/x86_64-linux-gnu/glib-2.0/include
	/usr/local/include
)

# Find required libraries (e.g., glib-2.0, bluetooth)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(BLUETOOTH REQUIRED bluez)

# Add the gattlib dependency
set(GATTLIB_BUILD_EXAMPLES NO CACHE BOOL "Don't build the GattLib examples" FORCE)
set(GATTLIB_SHARED_LIB NO CACHE BOOL "Build GattLib as a static library" FORCE)
set(GATTLIB_INSTALL NO CACHE BOOL "Exclude GattLib from the packaging process" FORCE)
add_subdirectory(external/gattlib)

# Add the executable
add_executable(ghlble ${SOURCES})

# Link libraries to the executable
target_link_libraries(ghlble
	gattlib
	${GLIB_LDFLAGS}
	${BLUETOOTH_LDFLAGS}
	-Wl,-rpath,'.'
)

# Add include directories to target
target_include_directories(ghlble PRIVATE
	${GLIB_INCLUDE_DIRS}
	${BLUETOOTH_INCLUDE_DIRS}
)

# Add compile definitions
target_compile_definitions(ghlble PRIVATE
	${GLIB_CFLAGS_OTHER}
	${BLUETOOTH_CFLAGS_OTHER}
)

# Packaging
set(CPACK_PACKAGE_INSTALL_DIRECTORY /usr CACHE STRING "Install directory (default: /usr).")
set(CPACK_PACKAGE_VERSION 1.0)
set(CPACK_PACKAGE_CONTACT "Black-Seraph <admin@black-seraph.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Guitar Hero Live Gamepad Connector")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${CPACK_PACKAGE_VERSION}_${CPACK_PACKAGE_ARCHITECTURE}")

#
# Debian, RPM and ZIP packages
#
if (CMAKE_SYSROOT)
	# CPack does like RPM package in a cross-toolchain context
	set(CPACK_GENERATOR "DEB;ZIP")
else()
	set(CPACK_GENERATOR "DEB;RPM;ZIP")
endif()

if (NOT CMAKE_SYSROOT)
# Detect platform architecture to use it for the Debian package
	execute_process(COMMAND dpkg --print-architecture OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE OUTPUT_QUIET)
endif()

set(CPACK_DEBIAN_PACKAGE_DEPENDS "libglib2.0-0")

# Bluez DBus API changed from v5.40
if (GATTLIB_DBUS AND (BLUEZ_VERSION_MAJOR EQUAL 5) AND (BLUEZ_VERSION_MINOR GREATER 39))
	set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, bluez (>= 5.40)")
endif()

# Install the executable binary
install(TARGETS ghlble
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}  # Typically /usr/bin
)

include(CPack)
