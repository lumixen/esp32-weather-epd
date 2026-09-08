FROM python:3.12-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends curl xz-utils ca-certificates git \
        libpixman-1-0 libglib2.0-0 libgcrypt20 libgnutls30 libpng16-16 libjpeg62-turbo \
        libsdl2-2.0-0 libepoxy0 libgl1 libx11-6 libslirp0 \
    && rm -rf /var/lib/apt/lists/*

# PlatformIO 6.2 pulls SCons 4.11, which is incompatible with the
# pioarduino platform used by this project.
RUN pip install --no-cache-dir platformio==6.1.19 pyyaml pydantic

COPY install_qemu.sh /opt/install_qemu.sh
RUN DEST=/opt/qemu /opt/install_qemu.sh

WORKDIR /project
CMD ["bash", "test/run_tests.sh"]
