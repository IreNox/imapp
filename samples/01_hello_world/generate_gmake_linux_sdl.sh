#/bin/bash

cd "$(dirname "$0")"
../../premake_tb --to=build/gmake_linux_sdl --use_sdl=on --os=linux --cc=gcc gmake2
if [ $? -ne 0 ]; then
  echo "Press any key to continue..."
  read -n 1
fi
