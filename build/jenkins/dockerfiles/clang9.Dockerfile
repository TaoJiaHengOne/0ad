FROM bullseye-base:latest

ARG DEBIAN_FRONTEND=noninteractive
ARG DEBCONF_NOWARNINGS="yes"
RUN apt-get update && apt-get install -qqy llvm-9 clang-9 lld-9 libclang-9-dev --no-install-recommends

ENV CC=clang-9
ENV CXX=clang++-9
ENV LDFLAGS="-fuse-ld=lld-9"
