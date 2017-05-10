#!/bin/bash
# Install dependencies

set -e

apt-get update -qq
apt-get install -y --no-install-recommends \
    git \
    cmake \
    libboost-all-dev \
    libcln-dev \
    libgmp-dev \
    libginac-dev \
    automake \
    libglpk-dev \
    libhwloc-dev