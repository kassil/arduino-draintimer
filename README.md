## Install dependencies (root acces)

Install docker as needed:

  sudo apt update
  sudo apt install docker.io
  sudo usermod -aG docker $USER
  newgrp docker

## Building the Container

docker build -t arduino-cmake .

## Running the Container

docker run --rm -it \
    -v $(pwd):/home/builder/project \
    arduino-cmake /bin/bash

## Build the project

cmake -B build -DCMAKE_TOOLCHAIN_FILE=avr-gcc-toolchain.cmake
cmake --build build
cmake --build build --target flash

