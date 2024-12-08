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
RUN apt install -y lsb-release wget software-properties-common gnupg
RUN wget https://apt.llvm.org/llvm.sh
RUN chmod +x llvm.sh
RUN ./llvm.sh 18
RUN apt-get -y install libclang-18-dev
RUN clang-18 --version
