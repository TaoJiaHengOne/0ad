FROM buster-base:latest

ARG DEBIAN_FRONTEND=noninteractive
ARG DEBCONF_NOWARNINGS="yes"
RUN apt-get install -qqy gcc-8 g++-8 --no-install-recommends

ENV LIBCC=gcc-8
ENV LIBCXX=g++-8
ENV CC=gcc-8
ENV CXX=g++-8
