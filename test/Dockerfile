FROM python:3.12-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends curl xz-utils ca-certificates git \
        libpixman-1-0 libglib2.0-0 libgcrypt20 libgnutls30 libpng16-16 libjpeg62-turbo \
        libsdl2-2.0-0 libepoxy0 libgl1 libx11-6 libslirp0 \
    && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir platformio pyyaml pydantic

COPY install_qemu.sh /opt/install_qemu.sh
RUN DEST=/opt/qemu /opt/install_qemu.sh

WORKDIR /project
CMD ["pio", "test", "-e", "lolin_d32_qemu", "--without-uploading"]
