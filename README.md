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

Generate the project's build system.

    cmake -B build -DCMAKE_TOOLCHAIN_FILE=avr-gcc-toolchain.cmake

Build the project.

    cmake --build build

Flash the target.

    cmake --build build --target flash

## Check the Flash and RAM usage

    avr-size -C --mcu=atmega328p build/ArduinoProject.elf
