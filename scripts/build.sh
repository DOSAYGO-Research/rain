#!/bin/bash

if [ ! -f src/cxxopts.hpp ]; then
  curl -L https://raw.githubusercontent.com/jarro2783/cxxopts/eb787304d67ec22f7c3a184ee8b4c481d04357fd/include/cxxopts.hpp -o src/cxxopts.hpp
fi

export PATH="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin:${PATH}"

make

