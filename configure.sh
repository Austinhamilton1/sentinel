#!/bin/bash

CC="gcc"
PREFIX=""

function help {
    echo "Usage: configure.sh --CC [compiler] --prefix [install path]"
    echo ""
    echo "--CC - the path to the compiler you want to use for building (defaults to gcc)"
    echo "--prefix - where you want sentinel to be installed"
    echo ""
    exit
}

function missingError {
    echo "--$1 requires an argument"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        '-h' | '--help')
            shift
            help
            ;;
        '--CC')
            shift
            CC=$1
            ;;
        '--prefix')
            shift
            PREFIX=$1
            ;;
        *)
            help
            ;;
    esac
    shift
done

if [ -z "$CC" ]; then
    echo "Missing compiler"
    exit 1
fi

if [ -z "$PREFIX" ]; then 
    echo "Missing prefix"
    exit 1
fi

if [ ! -d "$PREFIX" ]; then
    echo "Invalid prefix"
    exit 1
fi

cat > .config.mk <<EOF
CC = $CC
PREFIX = $PREFIX
EOF

echo "Configuration successful"