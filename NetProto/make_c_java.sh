#!/bin/bash

function message_abort {
  echo >&2 "You need to compile and install proto compiler: https://github.com/alexzk1/protocol-compiler"; exit 1;
}

function test_pc {
   command -v pc >/dev/null 2>&1 || { message_abort; }
   local tmp=$(pc -V)
   if [[ $tmp != *"pc version:"* ]]; then         
      message_abort
   fi
}

rm broadcast.cpp
rm broadcast.h
rm broadcast.java

test_pc
echo 'Going to compile network protocol'

pc -l c++ --c++-17 broadcast.proto
pc -l java --java-use-pkg biz.an_droid.desktopbroadcast.proto broadcast.proto

mv ./broadcast.java ../Android/DesktopBroadcast/app/src/main/java/biz/an_droid/desktopbroadcast/proto/

echo 'Network protocol is compiled'