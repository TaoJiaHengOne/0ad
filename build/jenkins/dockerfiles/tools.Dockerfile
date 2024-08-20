FROM python:3.12-slim-bookworm

ARG DEBIAN_FRONTEND=noninteractive
ARG DEBCONF_NOWARNINGS="yes"
RUN apt -qq update && apt install -qqy --no-install-recommends \
      git \
      git-lfs \
      subversion \
 && apt clean

RUN git lfs install --system --skip-smudge

RUN pip3 install --upgrade lxml
