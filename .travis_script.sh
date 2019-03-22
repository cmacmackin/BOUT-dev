#!/bin/bash

set -e

#Default flags
COVERAGE=0
UNIT=0
INTEGRATED=0
MMS=0
TESTS=0
MAIN_TARGET=

usage() {
    echo "$0 options are: "
    #Just pull out special comments from this file
    grep "\#\#\#" $0
    exit 1
}

#Handle input flags
while getopts "cuimt:" arg;
do
    case $arg in
	c) ### Run the coverage-post job tasks
	    COVERAGE=1
	    ;;
	u) ### Run the unit tests
	    UNIT=1
	    TESTS=1
	    ;;
	i) ### Run the integrated tests
	    INTEGRATED=1
	    TESTS=1
	    ;;
	m) ### Run the mms tests
	    MMS=1
	    TESTS=1
	    ;;
    t) ### Set target to build
        MAIN_TARGET+=("$OPTARG")
        ;;
	*) ### Show usage message
	    usage
	    ;;
    esac
done

echo "****************************************"
if [[ -d $HOME/local]]; then
    ls -Alh -$HOME/local
    if [[ -d $HOME/local/include ]]; then
        ls -Alh $HOME/local/include
    fi
    if [[ -d $HOME/local/lib ]]; then
        ls -Alh $HOME/local/lib
    fi
fi
echo "****************************************"

if [[ ! -d $HOME/local/include/sundials ]]; then
    echo "****************************************"
    echo "Building SUNDIALS"
    echo "****************************************"
    wget https://computation.llnl.gov/projects/sundials/download/sundials-4.1.0.tar.gz
    tar xvf sundials-4.1.0.tar.gz
    mkdir -p sundials-4.1.0/build && cd sundials-4.1.0/build
    cmake -DCMAKE_INSTALL_PREFIX=$HOME/local \
          -DMPI_ENABLE=on \
          -DOPENMP_ENABLE=on \
          -DMPI_C_COMPILER=$(which mpicc) \
          -DMPI_CXX_COMPILER=$(which mpic++) \
          -DMPIEXEC_EXECUTABLE=$(which mpiexec) \
          ..
    make && make install
    export LD_LIBRARY_FLAGS=$HOME/local/lib
    echo "****************************************"
    echo "Finished building SUNDIALS"
    echo "****************************************"
else
    echo "****************************************"
    echo "SUNDIALS already installed"
    echo "****************************************"
fi

export MAKEFLAGS="-j 2 -k"
echo "****************************************"
echo "Configuring with $CONFIGURE_OPTIONS"
echo "****************************************"
conf=0
time ./configure $CONFIGURE_OPTIONS MAKEFLAGS="$MAKEFLAGS" || conf=$?
if test $conf -gt 0
then
    RED_FG="\033[031m"
    RESET_FG="\033[039m"
    echo -e $RED_FG
    echo "**************************************************"
    echo "Printing config.log:"
    echo "**************************************************"
    echo -e $RESET_FG
    echo
    cat config.log
    echo
    echo -e $RED_FG
    echo "**************************************************"
    echo "Printing config-build.log:"
    echo "**************************************************"
    echo -e $RESET_FG
    echo
    cat config-build.log
    exit $conf
fi
export PYTHONPATH=$(pwd)/tools/pylib/:$PYTHONPATH

for target in ${MAIN_TARGET[@]}
do
    make_exit=0
    time make $target || make_exit=$?
    if [[ $make_exit -gt 0 ]]; then
	make clean > /dev/null
	echo -e $RED_FG
	echo "**************************************************"
	echo "Printing make commands:"
	echo "**************************************************"
	echo -e $RESET_FG
	echo
	make -n $target
	exit $make_exit
    fi
done

if [[ ${TESTS} == 1 ]]
then
    time make build-check
fi

if [[ ${UNIT} == 1 ]]
then
    time make check-unit-tests
fi

if [[ ${INTEGRATED} == 1 ]]
then
    time make check-integrated-tests
    time py.test-3 tools/pylib/
fi

if [[ ${MMS} == 1 ]]
then
   time make check-mms-tests
fi

if [[ ${COVERAGE} == 1 ]]
then
    # Ensure that there is a corresponding .gcda file for every .gcno file
    # This is to try and make the coverage report slightly more accurate
    # It still won't include, e.g. any solvers we don't build with though
    find . -name "*.gcno" -exec sh -c 'touch -a "${1%.gcno}.gcda"' _ {} \;

    # Use lcov to generate a report, upload it to codecov.io
    make code-coverage-capture
    bash <(curl -s https://codecov.io/bash) -f bout-coverage.info

    #For codacy
    bash ./.codacy_coverage.sh
fi
