FROM debian:buster-slim

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
      libwxgtk3.0-dev \
      libxml2-dev \
      llvm \
      make \
      m4 \
      patch \
      python3-dev \
      python3-pip \
      subversion \
      xz-utils \
      zlib1g-dev \
 && apt-get clean

# From bullseye forward this is done by installing the package python-is-python3
RUN ln -s /usr/bin/python3 /usr/bin/python

# Install git-lfs
RUN curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh | bash
RUN apt-get -qqy update && apt-get install -qqy --no-install-recommends git-lfs
RUN git lfs install --system --skip-smudge

# Install rust and Cargo via rustup
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- --default-toolchain 1.66.0 -y
ENV PATH="${PATH}:/root/.cargo/bin"

ENV SHELL=/bin/bash

