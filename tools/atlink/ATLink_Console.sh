#!/bin/sh

FILENAME="$PWD/libATLINKLIB.so"
if [ ! -e "$FILENAME" ]; then
rm $PWD/libATLINKLIB.so
ln -s $PWD/libATLINKLIB.so.1.0.0 $PWD/libATLINKLIB.so
fi

FILENAME="$PWD/libATLINKLIB.so.1"
if [ ! -e "$FILENAME" ]; then
rm $PWD/libATLINKLIB.so.1
ln -s $PWD/libATLINKLIB.so.1.0.0 $PWD/libATLINKLIB.so.1
fi

FILENAME="$PWD/libQt5Core.so.5"
if [ ! -e "$FILENAME" ]; then
rm $PWD/libQt5Core.so.5
  ln -s $PWD/libQt5Core.so.5.9.0 $PWD/libQt5Core.so.5
fi

FILENAME="$PWD/libicui18n.so.56"
if [ ! -e "$FILENAME" ]; then
rm $PWD/libicui18n.so.56
  ln -s $PWD/libicui18n.so.56.1 $PWD/libicui18n.so.56
fi

FILENAME="$PWD/libicudata.so.56"
if [ ! -e "$FILENAME" ]; then
rm $PWD/libicudata.so.56
  ln -s $PWD/libicudata.so.56.1 $PWD/libicudata.so.56
fi

FILENAME="$PWD/libicuuc.so.56"
if [ ! -e "$FILENAME" ]; then
rm $PWD/libicuuc.so.56
  ln -s $PWD/libicuuc.so.56.1 $PWD/libicuuc.so.56
fi
