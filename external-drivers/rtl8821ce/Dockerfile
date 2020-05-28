FROM ubuntu:latest AS build

ENV WORKDIR /build/

RUN mkdir -p $WORKDIR

WORKDIR $WORKDIR

RUN apt-get update

RUN apt-get install -y bc build-essential dkms linux-headers-$(uname -r)

FROM build

ADD . .

CMD /bin/bash