FROM debian:bullseye-slim

RUN useradd -ms /bin/bash --uid 1006 builder

# 0 A.D. dependencies.
ARG DEBIAN_FRONTEND=noninteractive
ARG DEBCONF_NOWARNINGS="yes"
RUN apt-get -qqy update && apt-get install -qqy --no-install-recommends \
      cmake \
      curl \
      libboost-dev \
      libboost-filesystem-dev \
      libcurl4-gnutls-dev \
      libenet-dev \
      libfmt-dev \
      libfreetype6-dev \
      libgloox-dev \
      libgnutls28-dev \
      libgtk-3-dev \
      libicu-dev \
      libminiupnpc-dev \
      libogg-dev \
      libopenal-dev \
      libpng-dev \
      libsdl2-dev \
      libsodium-dev \
      libvorbis-dev \
      libwxgtk3.0-gtk3-dev \
      libxml2-dev \
      make \
      m4 \
      patch \
      python3-dev \
      python3-pip \
      python-is-python3 \
      subversion \
      xz-utils \
      zlib1g-dev \
 && apt-get clean

# Install git-lfs
RUN curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh | bash
RUN apt-get -qqy update && apt-get install -qqy --no-install-recommends git-lfs
RUN git lfs install --system --skip-smudge

# Install rust and Cargo via rustup
USER builder
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- --default-toolchain 1.66.0 -y
ENV PATH="${PATH}:/home/builder/.cargo/bin"
USER root

ENV SHELL=/bin/bash
