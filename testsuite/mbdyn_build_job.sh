#!/bin/bash -f

# MBDyn (C) is a multibody analysis code.
# http://www.mbdyn.org
#
# Copyright (C) 1996-2023
#
# Pierangelo Masarati	<pierangelo.masarati@polimi.it>
# Paolo Mantegazza	<paolo.mantegazza@polimi.it>
#
# Dipartimento di Ingegneria Aerospaziale - Politecnico di Milano<
# via La Masa, 34 - 20156 Milano, Italy
# http://www.aero.polimi.it
#
# Changing this copyright notice is forbidden.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation (version 2 of the License).
#
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# AUTHOR: Reinhard Resch <mbdyn-user@a1.net>
# Copyright (C) 2023(-2023) all rights reserved.

# The copyright of this code is transferred
# to Pierangelo Masarati and Paolo Mantegazza
# for use in the software MBDyn as described
# in the GNU Public License version 2.1

program_name="$0"

program_dir=$(realpath $(dirname "${program_name}"))

if ! test -f "${program_dir}/mbdyn_build_job.sh"; then
    program_dir=$(realpath $(which "${program_name}"))
fi

## FIXME: On some systems it is called "lib64" whereas it is called "lib" on other systems.
## FIXME: In any case "lib64" should take precedence over "lib".
LIBDIR64="${LIBDIR64:-lib64 lib}"
MBD_SOURCE_DIR=${MBD_SOURCE_DIR:-`dirname ${program_dir}`}
MBD_SKIP_BUILD="${MBD_SKIP_BUILD:-no}"
MBD_INSTALL_PREFIX="${MBD_INSTALL_PREFIX:-${program_dir}/var/cache/mbdyn}"
MBD_BUILD_DIR="${MBD_BUILD_DIR:-${program_dir}/var/tmp/build/mbdyn}"
MBD_TEST_PROGS_OUTPUT_DIR="${MBD_TEST_PROGS_OUTPUT_DIR:-${program_dir}/var/tmp/tests/mbdyn-test-progs}"
MBD_COMPILER_FLAGS="${MBD_COMPILER_FLAGS:--Ofast -Wall -march=native -mtune=native -Wno-unused-variable}"
SICONOS_INSTALL_PREFIX="${SICONOS_INSTALL_PREFIX:-${program_dir}/var/cache/siconos}"
HDF5_INSTALL_PREFIX="${HDF5_INSTALL_PREFIX:-${program_dir}/var/cache/hdf5}"
NC_INSTALL_PREFIX="${NC_INSTALL_PREFIX:-${program_dir}/var/cache/netcdf}"
NC_CXX4_INSTALL_PREFIX="${NC_CXX4_INSTALL_PREFIX:-${program_dir}/var/cache/netcdf-cxx4}"
GTEST_INSTALL_PREFIX="${GTEST_INSTALL_PREFIX:-${program_dir}/var/cache/gtest}"
MKL_INSTALL_PREFIX="${MKL_INSTALL_PREFIX:-/usr/lib}"
MKL_PKG_CONFIG="${MKL_PKG_CONFIG:-mkl-dynamic-lp64-gomp}"
OCT_PKG_INSTALL_PREFIX="${OCT_PKG_INSTALL_PREFIX:-${program_dir}/var/cache/share/octave}"
MBD_WITH_MODULE="${MBD_WITH_MODULE:-fabricate damper-gandhi pid hfelem fab-electric template2 cont-contact wheel4 mds indvel mcp_test1 scalarfunc muscles minmaxdrive drive-test loadinc cudatest randdrive imu convtest md autodiff_test rotor-loose-coupling namespace drive controller constlaw fab-sbearings rotor_disc hunt-crossley diff damper-hydraulic cyclocopter fab-motion flightgear hid ns damper-graall nonsmooth-node cosim-output}"
MBD_NUM_BUILD_JOBS="${MBD_NUM_BUILD_JOBS:-$(($(lscpu | awk '/^Socket\(s\)/{ print $2 }') * $(lscpu | awk '/^Core\(s\) per socket/{ print $4 }')))}"
MBD_CONFIGURE_FLAGS="${MBD_CONFIGURE_FLAGS:---enable-python --enable-octave --enable-octave-utils --enable-install_test_progs --enable-netcdf --with-lapack --with-arpack --with-umfpack --with-klu --with-suitesparseqr --with-static-modules --without-mpi --enable-runtime-loading --enable-Werror --with-trilinos --with-siconos --with-gtest --enable-override-operator-new --enable-multithread}"
OCTAVE_MKOCTFILE="${MKOCTFILE:-mkoctfile}"
OCTAVE_CLI="${OCTAVE_CLI:-octave-cli}"
TRILINOS_INSTALL_PREFIX="${TRILINOS_INSTALL_PREFIX:-/usr}"
TRILINOS_INC_DIR="${TRILINOS_INC_DIR:-${TRILINOS_INSTALL_PREFIX}/include/trilinos}"
SUITESPARSE_INC_DIR="${SUITESPARSE_INC_DIR:-/usr/include/suitesparse}"
TFEL_LIB_DIR="${TFEL_LIB_DIR:-${TFEL_INSTALL_PREFIX}/lib}"
MGIS_INC_DIR="${MGIS_INC_DIR:-${MGIS_INSTALL_PREFIX}/include}"
MGIS_LIB_DIR="${MGIS_LIB_DIR:-${MGIS_INSTALL_PREFIX}/lib}"
GALLERY_LIB_DIR="${GALLERY_LIB_DIR:-${GALLERY_INSTALL_PREFIX}/lib}"


for libdir in ${LIBDIR64}; do
    NUMPY_INC_DIR="${NUMPY_INC_DIR:-/usr/${libdir}/python3.11/site-packages/numpy/core/include}"
    if ! test -d "${NUMPY_INC_DIR}"; then
        NUMPY_INC_DIR=
    fi
done
PYTHON_INC_DIR="${PYTHON_INC_DIR:-`python3-config --includes`}"
PYTHON_LDFLAGS="${PYTHON_LDFLAGS:-`python3-config --ldflags`}"
PYTHON_INC_DIR="${PYTHON_INC_DIR:-`python-config --includes`}" ## Just in case "python3-config" is called "python-config"
PYTHON_LDFLAGS="${PYTHON_LDFLAGS:-`python-config --ldflags`}"
## CXXFLAGS="${CXXFLAGS:--Wno-overloaded-virtual}" ## FIXME: Any idea on how to fix this warning is welcome!
## CFLAGS="${CFLAGS:--Wno-error=lto-type-mismatch}" ## FIXME: Needed for utils/femgen_f.f and utils/test_strext_socket_f.f

## FIXME: Enable C++20 because it is required by MFrontGenericInterfaceSupport
CXXFLAGS="${CXXFLAGS:--std=c++20}"
MBD_CLEAN_BUILD="${MBD_CLEAN_BUILD:-no}"
MBD_FORCE_CONFIGURE="${MBD_FORCE_CONFIGURE:-no}"
MBD_CLEAN_ALL="${MBD_CLEAN_ALL:-no}"

while ! test -z "$1"; do
    case "$1" in
        --mbdyn-install-prefix)
            MBD_INSTALL_PREFIX="$2"
            shift
            ;;
        --mbdyn-source-dir)
            MBD_SOURCE_DIR="$2"
            shift
            ;;
        --mbdyn-build-dir)
            MBD_BUILD_DIR="$2"
            shift
            ;;
        --mbdyn-compiler-flags)
            MBD_COMPILER_FLAGS="$2"
            shift
            ;;
        --mbdyn-linker-flags)
            LDFLAGS="$2"
            shift
            ;;
        --mbdyn-clean-build)
            MBD_CLEAN_BUILD="$2"
            shift
            ;;
        --mbdyn-force-configure)
            MBD_FORCE_CONFIGURE="$2"
            shift
            ;;
        --siconos-install-prefix)
            SICONOS_INSTALL_PREFIX="$2"
            shift
            ;;
        --netcdf-install-prefix)
            NC_INSTALL_PREFIX="$2"
            shift
            ;;
        --netcdf-cxx4-install-prefix)
            NC_CXX4_INSTALL_PREFIX="$2"
            shift
            ;;
        --gtest-install-prefix)
            GTEST_INSTALL_PREFIX="$2"
            shift
            ;;
        --trilinos-install-prefix)
            TRILINOS_INSTALL_PREFIX="$2"
            TRILINOS_INC_DIR="${TRILINOS_INSTALL_PREFIX}/include"
            LDFLAGS="-L${TRILINOS_INSTALL_PREFIX}/lib -Wl,-rpath=${TRILINOS_INSTALL_PREFIX}/lib ${LDFLAGS}"
            shift
            ;;
        --mkl-install-prefix)
            MKL_INSTALL_PREFIX="$2"
            shift
            ;;
        --mkl-pkg-config)
            MKL_PKG_CONFIG="$2"
            shift
            ;;
        --octave-pkg-install-prefix)
            OCT_PKG_INSTALL_PREFIX="$2"
            shift
            ;;
        --octave-mkoctfile)
            OCTAVE_MKOCTFILE="$2"
            shift
            ;;
        --octave-cli)
            OCTAVE_CLI="$2"
            shift
            ;;
        --configure-flags)
            MBD_CONFIGURE_FLAGS="$2"
            shift
            ;;
        --configure-flags-add)
            MBD_CONFIGURE_FLAGS="${MBD_CONFIGURE_FLAGS} $2"
            shift
            ;;
        --tasks)
            MBD_NUM_BUILD_JOBS="$2"
            shift
            ;;
        --help)
            echo "${program_name} --mbdyn-install-prefix <MBD_INSTALL_PREFIX>"
            echo "                --mbdyn-build-dir <MBD_BUILD_DIR>"
            echo "                --mbdyn-install-prefix <MBD_INSTALL_PREFIX>"
            echo "                --mbdyn-compiler-flags <MBD_COMPILER_FLAGS>"
            echo "                --siconos-install-prefix <SICONOS_INSTALL_PREFIX>"
            echo "                --netcdf-install-prefix <NC_INSTALL_PREFIX>"
            echo "                --netcdf-cxx4-install-prefix <NC_CXX4_INSTALL_PREFIX>"
            echo "                --mkl-install-prefix <MKL_INSTALL_PREFIX>"
            echo "                --octave-pkg-install-prefix <OCT_PKG_INSTALL_PREFIX>"
            echo "                --trilinos-install-prefix <TRILINOS_INSTALL_PREFIX>"
            exit 1
            ;;
        *)
            echo "${program_name}: invalid argument \"$1\""
            exit 1
            ;;
    esac
    shift
done

if test -z "${MBD_INSTALL_PREFIX}"; then
    echo "${program_name} missing argument --mbdyn-install-prefix"
    exit 1
fi

if test -z "${MBD_BUILD_DIR}"; then
    echo "${program_name} missing argument --mbdyn-build-dir"
    exit 1
fi

if test -z "${SICONOS_INSTALL_PREFIX}"; then
    echo "${program_name} missing argument --siconos-install-prefix"
    exit 1
fi

if test -z "${NC_INSTALL_PREFIX}"; then
    echo "${program_name} missing argument --netcdf-install-prefix"
    exit 1
fi

if test -z "${NC_CXX4_INSTALL_PREFIX}"; then
    echo "${program_name} missing argument --netcdf-cxx4-install-prefix"
    exit 1
fi

if test -z "${MKL_INSTALL_PREFIX}"; then
    echo "${program_name} missing argument --mkl-install-prefix"
    exit 1
fi

if test -z "${OCT_PKG_INSTALL_PREFIX}"; then
    echo "${program_name} missing argument --octave-pkg-install-prefix"
    exit 1
fi

if ! mkdir -p "${OCT_PKG_INSTALL_PREFIX}"; then
    echo "Failed to create directory ${OCT_PKG_INSTALL_PREFIX}"
    exit 1
fi

if "${MBD_INSTALL_PREFIX}/bin/mbdyn" --version >& /dev/null && test "${MBD_SKIP_BUILD}" = "yes"; then
    echo "Do not build MBDyn because it was already installed and MBD_SKIP_BUILD=yes"
    exit 0
fi

if ! test -d "${MBD_SOURCE_DIR}"; then
    echo "Source directory ${MBD_SOURCE_DIR} does not exit"
    exit 1
fi

MBD_SOURCE_DIR=`realpath ${MBD_SOURCE_DIR}`

## Let's try to speed up the build process a bit
## Execute bootstrap.sh and configure only if needed
## This conditional execution may be removed as soon as the testsuite is finished
if ! test -x ./configure ; then
    chmod u+x ./bootstrap.sh

    if ! ./bootstrap.sh; then
        echo "bootstrap.sh failed"
        exit 1
    fi
fi

if ! test -x ./configure; then
    echo "configure not found"
    exit 1
fi

echo "Create build directory ..."
if test ${MBD_CLEAN_BUILD} = "yes" -o ${MBD_CLEAN_ALL} = "yes"; then
    echo "cleanup build directory ..."
    rm -rf ${MBD_BUILD_DIR}
fi

echo "create build directory ..."

if ! mkdir -p "${MBD_BUILD_DIR}"; then
    echo "Failed to create build directory"
    exit 1
fi

if ! mkdir -p "${MBD_INSTALL_PREFIX}"; then
    echo "Failed to create installation directory"
    exit 1
fi

cd "${MBD_BUILD_DIR}"

export PATH="${NC_INSTALL_PREFIX}/bin:${NC_CXX4_INSTALL_PREFIX}/bin:${HDF5_INSTALL_PREFIX}/bin:${PATH}"

if test -d "${SUITESPARSE_INC_DIR}"; then
    CPPFLAGS="-I${SUITESPARSE_INC_DIR} ${CPPFLAGS}"
fi

if test -d "${TRILINOS_INSTALL_PREFIX}"; then
    TRILINOS_INC_DIR="${TRILINOS_INC_DIR:-${TRILINOS_INSTALL_PREFIX}/include}"

    if test -d "${TRILINOS_INC_DIR}"; then
        CPPFLAGS="-I${TRILINOS_INC_DIR} ${CPPFLAGS}"
    fi

    TRILINOS_LIB_DIR="${TRILINOS_LIB_DIR:-${TRILINOS_INSTALL_PREFIX}/lib}"

    if test -d "${TRILINOS_LIB_DIR}"; then
        LDFLAGS="-L${TRILINOS_LIB_DIR} -Wl,-rpath=${TRILINOS_LIB_DIR} ${LDFLAGS}"
    fi
fi

if test -d "${NUMPY_INC_DIR}"; then
    CPPFLAGS="-I${NUMPY_INC_DIR} ${CPPFLAGS}"
fi

if test -d "${PYTHON_INC_DIR}"; then
    CPPFLAGS="${PYTHON_INC_DIR} ${CPPFLAGS}"
    LDFLAGS="${PYTHON_LDFLAGS} ${LDFLAGS}"
fi

if test -d "${SICONOS_INSTALL_PREFIX}"; then
    SICONOS_INC_DIR="${SICONOS_INC_DIR:-${SICONOS_INSTALL_PREFIX}/include/siconos}"
    for libdir in ${LIBDIR64}; do
        SICONOS_LIB_DIR="${SICONOS_LIB_DIR:-${SICONOS_INSTALL_PREFIX}/${libdir}}"
        if ! test -d "${SICONOS_LIB_DIR}"; then
            SICONOS_LIB_DIR=
        fi
    done
    CPPFLAGS="-I${SICONOS_INC_DIR} ${CPPFLAGS}"
    if ! test -z "${SICONOS_LIB_DIR}"; then
        LDFLAGS="-L${SICONOS_LIB_DIR} -Wl,-rpath=${SICONOS_LIB_DIR} ${LDFLAGS}"
    fi
fi

echo "Detecting HDF5 ..."
if `which h5cc >& /dev/null`; then
    HDF5_INCDIR=${HDF5_INSTALL_PREFIX}/include
    HDF5_LIBDIR=${HDF5_INSTALL_PREFIX}/lib64
    CPPFLAGS="${CPPFLAGS} -I${HDF5_INCDIR}"
    LDFLAGS="-L${HDF5_LIBDIR} -Wl,-rpath=${HDF5_LIBDIR} ${LDFLAGS}"
else
    echo "Warning: hdf5 was not found in ${HDF5_INSTALL_PREFIX}!"
fi

echo "Detecting NetCDF ..."
if nc-config --help >& /dev/null; then
    NC_INCDIR=`nc-config --includedir`
    NC_LIBDIR=`nc-config --libdir`
    CPPFLAGS="${CPPFLAGS} -I${NC_INCDIR}"
    LDFLAGS="-L${NC_LIBDIR} -Wl,-rpath=${NC_LIBDIR} ${LDFLAGS}"
else
    echo "Warning: nc-config was not found in ${NC_INSTALL_PREFIX}!"
fi

echo "Detecting NetCDF-cxx4 ..."
if ncxx4-config --help >& /dev/null; then
    NC_INCDIR=`ncxx4-config --includedir`
    NC_LIBDIR=`ncxx4-config --libdir`
    CPPFLAGS="${CPPFLAGS} -I${NC_INCDIR}"
    LDFLAGS="-L${NC_LIBDIR} -Wl,-rpath=${NC_LIBDIR} ${LDFLAGS}"
else
    echo "Warning: ncxx4-config was not found in ${NC_CXX4_INSTALL_PREFIX}!"
fi

echo "Detecting MKL ..."
if test -d "${MKL_INSTALL_PREFIX}"; then
    echo "Find all pkg-config files in ${MKL_INSTALL_PREFIX} ..."
    echo "Search for ${MKL_PKG_CONFIG}.pc ..."
    MKL_PKG_CONFIG_FILE=`find "${MKL_INSTALL_PREFIX}" '(' -type f -and -name "${MKL_PKG_CONFIG}.pc" ')'`
    if test -f "${MKL_PKG_CONFIG_FILE}"; then
        MKL_PKG_CONFIG_PATH=`dirname "${MKL_PKG_CONFIG_FILE}"`
        echo "MKL_PKG_CONFIG_PATH=${MKL_PKG_CONFIG_PATH}"
    else
        echo "File ${MKL_PKG_CONFIG}.pc not found"
    fi
else
    echo "Warning: MKL was not found in ${MKL_INSTALL_PREFIX}!"
fi

PARDISO_FLAGS="--without-pardiso"
if test -d "${MKL_PKG_CONFIG_PATH}"; then
    export PKG_CONFIG_PATH="${MKL_PKG_CONFIG_PATH}:${PKG_CONFIG_PATH}"
    if pkg-config --validate "${MKL_PKG_CONFIG}"; then
        PARDISO_FLAGS="--with-pardiso"
        CPPFLAGS="${CPPFLAGS} `pkg-config --cflags-only-I ${MKL_PKG_CONFIG}`"
        LDFLAGS="${LDFLAGS} `pkg-config --libs-only-L ${MKL_PKG_CONFIG}`"
        LDFLAGS="${LDFLAGS} `pkg-config --libs-only-L ${MKL_PKG_CONFIG} | sed  's/^-L\//-Wl,-rpath=\//g'`"
    else
        echo "pkg-config ${MKL_PKG_CONFIG} is not valid"
    fi
else
    echo "Warning: MKL_PKG_CONFIG_PATH could not be detected"
fi

if test -d "${GTEST_INSTALL_PREFIX}"; then
    GTEST_PKG_CONFIG=`find "${GTEST_INSTALL_PREFIX}" -name pkgconfig -and -type d`
    if test -d "${GTEST_PKG_CONFIG}"; then
        export PKG_CONFIG_PATH="${GTEST_PKG_CONFIG}:${PKG_CONFIG_PATH}"
        export LDFLAGS="${LDFLAGS} `pkg-config --libs-only-L gtest | sed 's/^-L\//-Wl,-rpath=\//g'`"
    else
        echo "Warning: GTEST_PKG_CONFIG_PATH could not be detected"
    fi
else
    echo "Warning: GTEST_PKG_CONFIG_PATH could not be detected"
fi

if ! test -z "${MBD_WITH_MODULE}"; then
    MBD_COMPILER_FLAGS="${MBD_COMPILER_FLAGS} -rdynamic" ## Needed for --enable-runtime-loading
fi

CPPFLAGS="${CPPFLAGS} -I${MGIS_INC_DIR}"
LDFLAGS="${LDFLAGS} -L${MGIS_LIB_DIR} -Wl,-rpath=${MGIS_LIB_DIR} -L${TFEL_LIB_DIR} -Wl,-rpath=${TFEL_LIB_DIR}"
echo CXXFLAGS="${MBD_COMPILER_FLAGS} ${CXXFLAGS}"
echo CPPFLAGS="${CPPFLAGS}"
echo LDFLAGS="${LDFLAGS}"
printf "configure ...\n"
if test "${MBD_FORCE_CONFIGURE}" != "no" || ! test -f Makefile || test "${program_dir}/mbdyn_build_job.sh" -nt Makefile || test "${MBD_SOURCE_DIR}/configure.ac" -nt Makefile; then
    echo "${MBD_BUILD_DIR}/Makefile does not exist or it might be out of date!"
    echo "Run configure step ..."
    if ! "${MBD_SOURCE_DIR}/configure" \
         PYTHON_VERSION=3 \
         CPPFLAGS="${CPPFLAGS}" \
         LDFLAGS="${LDFLAGS}" \
         CXXFLAGS="${MBD_COMPILER_FLAGS} ${CXXFLAGS}" \
         CFLAGS="${MBD_COMPILER_FLAGS} ${CFLAGS}" \
         FFLAGS="${MBD_COMPILER_FLAGS} ${FFLAGS}" \
         FCFLAGS="${MBD_COMPILER_FLAGS} ${FCFLAGS}" \
         --prefix="${MBD_INSTALL_PREFIX}" \
         --with-octave-pkg-prefix="${OCT_PKG_INSTALL_PREFIX}" \
         --with-octave-cli="${OCTAVE_CLI}" \
         --with-mkoctfile="${OCTAVE_MKOCTFILE}" \
         --with-mfront \
         ${PARDISO_FLAGS} \
         --with-module="${MBD_WITH_MODULE}" \
         ${MBD_CONFIGURE_FLAGS}  ; then
        echo "Failed to run  ${MBD_SOURCE_DIR}/configure"
        exit 1
    fi
else
    echo "${MBD_BUILD_DIR}/Makefile exists"
    echo "configure step will be skipped ..."
fi

if ! test -f Makefile; then
    echo "Makefile does not exist"
    exit 1
fi

printf "Compiling the code using %s jobs ...\n" ${MBD_NUM_BUILD_JOBS}
if ! make -j${MBD_NUM_BUILD_JOBS}; then
    echo "build failed"
    exit 1
fi

echo "Run built-in unit tests"
mkdir -p "${MBD_TEST_PROGS_OUTPUT_DIR}"

export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${GALLERY_LIB_DIR}:${TFEL_LIB_DIR}

if ! make MBD_TEST_PROGS_OUTPUT_DIR="${MBD_TEST_PROGS_OUTPUT_DIR}" test; then
    echo "Built-in unit tests failed"
    exit 1
fi

echo "Clean up local installation directory"

rm -rf ${MBD_INSTALL_PREFIX}

printf "Install the code in \"%s\"\n" "${MBD_INSTALL_PREFIX}"
if ! make install; then
    echo "installation failed"
    exit 1
fi

printf "Build mbdyn-*.tar.gz"
if ! make dist; then
    echo "make dist failed"
    exit 1
fi

echo "MBDyn version:"
if ! ${MBD_INSTALL_PREFIX}/bin/mbdyn --version; then
    echo "MBDyn's binary is not executable"
    exit 1
fi

echo "Shared libraries used by MBDyn:"
ldd ${MBD_INSTALL_PREFIX}/bin/mbdyn
