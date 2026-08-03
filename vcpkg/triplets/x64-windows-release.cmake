set(VCPKG_ENV_PASSTHROUGH_UNTRACKED FC PATH)

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_BUILD_TYPE release)

# Use vcpkg's MinGW gfortran when lapack-reference is not in the NuGet binary cache.
set(VCPKG_PROVIDED_FORTRAN ON)
