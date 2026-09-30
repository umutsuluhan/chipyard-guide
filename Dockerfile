FROM ubuntu:24.04

WORKDIR /opt

SHELL ["/bin/bash", "-c"]

RUN apt-get update && \
    apt-get install -y \
        software-properties-common \
        lsb-release \
        wget \
        vim \
        apt-utils \
        git \
        sudo \
        curl \
        kmod && \
    rm -rf /var/lib/apt/lists/*

RUN mkdir -p /root/.sbt && \
    printf '[repositories]\n  local\n  google-maven-central: https://maven-central.storage-download.googleapis.com/maven2/\n  maven-central\n  sbt-plugin-releases: https://repo.scala-sbt.org/scalasbt/sbt-plugin-releases/, [organization]/[module]/(scala_[scalaVersion]/)(sbt_[sbtVersion]/)[revision]/[type]s/[artifact](-[classifier]).[ext]\n  typesafe-ivy-releases: https://repo.typesafe.com/typesafe/ivy-releases/, [organization]/[module]/(scala_[scalaVersion]/)(sbt_[sbtVersion]/)[revision]/[type]s/[artifact](-[classifier]).[ext]\n' \
    > /root/.sbt/repositories

ENV SBT_OPTS="-Dsbt.override.build.repos=true -Dsbt.repository.config=/root/.sbt/repositories"

RUN wget "https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-$(uname)-$(uname -m).sh" && \
    bash Miniforge3-$(uname)-$(uname -m).sh -b -p /opt/conda && \
    rm Miniforge3-$(uname)-$(uname -m).sh

ENV PATH="/opt/conda/bin:$PATH"

WORKDIR /workspace

RUN git clone --branch 1.14.0 \
    https://github.com/ucb-bar/chipyard.git

WORKDIR /workspace/chipyard

RUN ./build-setup.sh riscv-tools \
    --skip-marshal \
    --skip-firesim

COPY course/configs/CourseRocketConfig.scala \
    /workspace/chipyard/generators/chipyard/src/main/scala/config/CourseRocketConfig.scala

COPY scripts/test-install.sh /workspace/test-install.sh
RUN chmod +x /workspace/test-install.sh

WORKDIR /workspace/chipyard
