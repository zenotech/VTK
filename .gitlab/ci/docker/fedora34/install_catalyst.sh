#!/bin/sh

readonly catalyst_repo="https://gitlab.kitware.com/paraview/catalyst"
# Use the latest release candidate.
readonly catalyst_commit="v2.0.0-rc3"

readonly catalyst_root="$HOME/catalyst"
readonly catalyst_src="$catalyst_root/src"
readonly catalyst_build_root="$catalyst_root/build"

git clone -b "$catalyst_commit" "$catalyst_repo" "$catalyst_src"

catalyst_build () {
    local subdir="$1"
    shift

    local prefix="$1"
    shift

    cmake -GNinja \
        -S "$catalyst_src" \
        -B "$catalyst_build_root/$subdir" \
        -DCATALYST_BUILD_SHARED_LIBS=ON \
        -DCATALYST_BUILD_TESTING=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        "-DCMAKE_INSTALL_PREFIX=$prefix" \
        "$@"
    cmake --build "$catalyst_build_root/$subdir" --target install
}

# MPI-less
catalyst_build nompi /usr \
    -DCATALYST_USE_MPI=OFF

# MPICH
catalyst_build mpich /usr/lib64/mpich \
    -DCATALYST_USE_MPI=ON \
    -DCMAKE_INSTALL_LIBDIR=lib

# OpenMPI
catalyst_build openmpi /usr/lib64/openmpi \
    -DCATALYST_USE_MPI=ON \
    -DCMAKE_INSTALL_LIBDIR=lib

rm -rf "$catalyst_root"
