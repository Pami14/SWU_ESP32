FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    gcc-avr \
    binutils-avr \
    avr-libc \
    avrdude \
    gdb \
    git

WORKDIR /workspace