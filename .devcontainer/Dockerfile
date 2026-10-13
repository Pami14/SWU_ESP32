FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV IDF_PATH=/opt/esp-idf
ENV IDF_TOOLS_PATH=/opt/esp-tools

RUN apt-get update && apt-get install -y --no-install-recommends \
    git wget flex bison gperf python3 python3-venv python3-pip \
    cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0 \
    ca-certificates gdb \
    && rm -rf /var/lib/apt/lists/*

RUN git clone -b v5.4 --depth 1 --recursive --shallow-submodules \
      https://github.com/espressif/esp-idf.git $IDF_PATH \
    && $IDF_PATH/install.sh esp32 \
    && rm -rf $IDF_TOOLS_PATH/dist

RUN echo '. /opt/esp-idf/export.sh' >> /root/.bashrc

WORKDIR /workspace