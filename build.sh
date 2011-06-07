#!/bin/sh
mkdir -p $1;
cd $1
if [ "$1" = "Debug" ]; then
echo "Building Debug Build";
../autogen.sh 'CFLAGS=-g -O0' 'CXXFLAGS=-g -O0' 'JFLAGS=-g -O0' 'FFLAGS=-g -O0' --prefix=/usr
elif [ "$1" = "Release" ]; then
echo "Building Release Build";
../autogen.sh 'CFLAGS=-O2' 'CXXFLAGS=-O2' 'JFLAGS=-O2' 'FFLAGS=-O2' --prefix=/usr
elif [ "$1" = "Profile" ]; then
echo "Building Profile Build";
../autogen.sh 'CFLAGS=-g -pg' 'CXXFLAGS=-g -pg' 'JFLAGS=-g -pg' 'FFLAGS=-g -pg' --prefix=/usr
fi
make
