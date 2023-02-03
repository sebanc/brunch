FROM ubuntu:latest AS build

ENV KVER=5.18.0-051800-generic
ARG URL_LINUX_HEADERS=https://kernel.ubuntu.com/~kernel-ppa/mainline/v5.18/amd64/linux-headers-5.18.0-051800_5.18.0-051800.202205222030_all.deb
ARG URL_LINUX_HEADERS_GENERIC=https://kernel.ubuntu.com/~kernel-ppa/mainline/v5.18/amd64/linux-headers-5.18.0-051800-generic_5.18.0-051800.202205222030_amd64.deb
ARG URL_LINUX_MODULES=https://kernel.ubuntu.com/~kernel-ppa/mainline/v5.18/amd64/linux-modules-5.18.0-051800-generic_5.18.0-051800.202205222030_amd64.deb
ARG URL_LINUX_IMAGE=https://kernel.ubuntu.com/~kernel-ppa/mainline/v5.18/amd64/linux-image-unsigned-5.18.0-051800-generic_5.18.0-051800.202205222030_amd64.deb

ENV WORKDIR /build/

RUN mkdir -p $WORKDIR

WORKDIR /debs

RUN apt-get update

RUN apt-get install -y bc build-essential dkms wget linux-base

RUN wget $URL_LINUX_HEADERS $URL_LINUX_HEADERS_GENERIC $URL_LINUX_MODULES $URL_LINUX_IMAGE

RUN dpkg -i ./*.deb
RUN apt-get install -f

WORKDIR $WORKDIR

FROM build

ADD . .

CMD /bin/bash
