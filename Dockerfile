FROM debian:bookworm-slim

# Install build tools and AVR toolchain
RUN apt-get update && apt-get install -y \
    cmake \
    make \
    gcc-avr \
    binutils-avr \
    avr-libc \
    git \
    nano \
    arduino-core-avr \
    && rm -rf /var/lib/apt/lists/*

# Optional: add a user so avrdude doesn’t run as root
RUN useradd -ms /bin/bash builder
USER builder
WORKDIR /home/builder/project

