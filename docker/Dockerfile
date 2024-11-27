FROM node:21-bookworm

RUN apt-get update

# Install wget
RUN apt-get install -y wget

# Install Git
RUN apt-get install -y git
RUN git --version

# Install CMake
RUN apt install -y cmake
RUN cmake --version

# Install g++
RUN apt install -y g++

# Install gdb
RUN apt install -y gdb

# Install clang/llvm 18
RUN apt-get update
RUN wget https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.8/clang+llvm-18.1.8-x86_64-linux-gnu-ubuntu-18.04.tar.xz
RUN tar -xf clang+llvm-18.1.8-x86_64-linux-gnu-ubuntu-18.04.tar.xz --skip-old-files --strip-components=1 -C /usr/
RUN apt install -y libncurses5
RUN clang --version
RUN rm clang+llvm-18.1.8-x86_64-linux-gnu-ubuntu-18.04.tar.xz
