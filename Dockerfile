FROM ubuntu:22.04
# Build with docker build -t daw-ci-build -f build-win32.dockerfile . --build-arg GITHUB_TOKEN=$GITHUB_TOKEN
ARG DEBIAN_FRONTEND=noninteractive
RUN mkdir /build
WORKDIR /build

RUN apt-get update -yqq
RUN apt-get install -qqy --no-install-recommends wget sudo curl \
    python3 zip unzip git libbsd0 libbsd-dev libtinfo6 libxml2  \
    libncursesw6 nsis libx11-dev libxrandr-dev libxinerama-dev  \
    libxcursor-dev libxi-dev libasound2-dev libgtk-3-dev bzip2  \
    libssl-dev gnupg2 e2fslibs-dev libatomic1 ca-certificates   \
    libattr1-dev chrpath libstdc++-12-dev  

# add user builder
RUN useradd -ms /bin/bash builder
# add user builder to sudoers
RUN echo "builder ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

ENV LLVM_LINUX_PATH=/opt/llvm-linux
ENV LLVM_MINGW_PATH=/opt/llvm-mingw
RUN wget --no-check-certificate -nv https://github.com/nidefawl/llvm-project/releases/download/v16.0.0-3-linux/llvm-clang-16.0.0-libc++-abi-2-ubuntu-22.04.bz2 && \
 tar xjf llvm-clang-*.bz2 && rm -f *.bz2 && rm -Rf $LLVM_LINUX_PATH && mv llvm-clang-* $LLVM_LINUX_PATH 
RUN wget --no-check-certificate -nv https://github.com/nidefawl/llvm-mingw/releases/download/v16.0.0-linux-1/llvm-mingw-clang-16.0.0-libc++-abi-2-ubuntu-22.04-x86_64.bz2  && \
 tar xjf llvm-mingw-*.bz2 && rm -f *.bz2 && rm -Rf $LLVM_MINGW_PATH && mv llvm-mingw-* $LLVM_MINGW_PATH
RUN wget --no-check-certificate -nv https://github.com/Kitware/CMake/releases/download/v3.25.0-rc4/cmake-3.25.0-rc4-linux-$(uname -m).tar.gz -O cmake-linux.tar.gz && \
 tar xzf cmake-linux.tar.gz && rm -f cmake-linux.tar.gz && rm -Rf /opt/cmake && mv cmake-* /opt/cmake
RUN wget --no-check-certificate -nv https://github.com/ninja-build/ninja/releases/download/v1.11.1/ninja-linux.zip -O ninja-linux.zip && unzip ninja-linux.zip -d /opt/cmake/bin && rm ninja-linux.zip

ENV STORED_PATH=$PATH

RUN echo 'set(PROJECT_PRODUCT_NAME "DAW" CACHE STRING "")\n\
set(PROJECT_BINARY_NAME "DAW" CACHE STRING "")\n\
set(DPRODUCT_HOST_NAME "DAW" CACHE STRING "")\n\
set(PROJECT_VENDOR_NAME "Michael Hept" CACHE STRING "")\n\
set(PRODUCT_URL_DOCS "https://github.com/nidefawl/daw-project" CACHE STRING "")\n\
set(PRODUCT_URL_VENDOR "https://github.com/nidefawl/daw-project" CACHE STRING "")\n' >> ./CommonConfig.cmake

RUN cat CommonConfig.cmake
ARG GITHUB_USER="doccker-builder"
ARG GITHUB_TOKEN

WORKDIR /build

RUN git config --global user.name $GITHUB_USER && \
    git config --global user.email root@localhost && \
    git config --global init.defaultBranch main && \
    git config --global advice.detachedHead false

RUN git clone --depth=1 --branch=master --single-branch https://${GITHUB_USER}:${GITHUB_TOKEN}@github.com/nidefawl/daw-deps.git daw-deps
RUN git -C daw-deps submodule update --init
RUN git clone --depth=1 --branch=master --single-branch https://${GITHUB_USER}:${GITHUB_TOKEN}@github.com/nidefawl/daw.git daw


RUN chown builder:builder /build

RUN chown builder:builder daw-deps -R
RUN chown builder:builder daw -R

# switch to user builder
USER builder
RUN mkdir -p bin && mkdir -p installer

ENV PATH=$LLVM_MINGW_PATH/bin:/opt/cmake/bin:$STORED_PATH
ENV CC=$LLVM_MINGW_PATH/bin/x86_64-w64-mingw32-clang
ENV CXX=$LLVM_MINGW_PATH/bin/x86_64-w64-mingw32-clang++

RUN echo "#include <stdio.h>\nint main() { printf(\"Hello World!\\\\n\"); return 0; }\n" > test.c
RUN $CC -o test.exe test.c 

RUN python3 ./daw-deps/build.py ./build-deps/win32 ./install-deps/win32 -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres -release


RUN echo "#!/bin/bash\n# DUMP INPUT ARGS\nfor i in \"\$@\"\ndo\n  echo \"\$i\"\ndone\nexit 1" > dump_args.sh
RUN cmake \
 -C CommonConfig.cmake \
 -S"daw" \
 -B"/build/build/win32" \
 -G"Ninja Multi-Config" \
 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
 -DPROJECT_DEPS_PATH:PATH=/build/daw-deps \
 -DPROJECT_DEPS_INSTALL_PATH:PATH=/build/install-deps/win32 \
 -DPROJECT_INSTALLER_OUTPUT_PATH:PATH=/build/installer/win32 \
 -DPROJECT_WORKING_DIR:PATH=/build/bin/win32 \
 -DPROJECT_VST2_OUTPUT_PATH:PATH=/build/bin/win32 \
 -DCMAKE_SYSTEM_NAME=Windows \
 -DCMAKE_C_FLAGS=-D_WIN32=1 \
 -DCMAKE_CXX_FLAGS=-D_WIN32=1 \
 -DPROJECT_BUILD_TESTS_UI=OFF \
 -DCMAKE_DISABLE_PRECOMPILE_HEADERS=OFF \
 -DCMAKE_VERBOSE_MAKEFILE=OFF \
 -DCMAKE_UNITY_BUILD=ON \
 -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF

RUN cmake --build /build/build/win32 --config RelWithDebInfo --target dist-installer -- -j 8

ENV PATH=$LLVM_LINUX_PATH/bin:/opt/cmake/bin:$STORED_PATH
ENV CC=$LLVM_LINUX_PATH/bin/clang
ENV CXX=$LLVM_LINUX_PATH/bin/clang++
ENV LD_LIBRARY_PATH=$LLVM_LINUX_PATH/lib/x86_64-unknown-linux-gnu

RUN $CC -o test.elf test.c 

RUN python3 ./daw-deps/build.py ./build-deps/linux ./install-deps/linux -release

RUN cmake \
 -C CommonConfig.cmake \
 -S"daw" \
 -B"/build/build/linux" \
 -G"Ninja Multi-Config" \
 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
 -DPROJECT_DEPS_PATH:PATH=/build/daw-deps \
 -DPROJECT_DEPS_INSTALL_PATH:PATH=/build/install-deps/linux \
 -DPROJECT_INSTALLER_OUTPUT_PATH:PATH=/build/installer/linux \
 -DPROJECT_WORKING_DIR:PATH=/build/bin/linux \
 -DPROJECT_VST2_OUTPUT_PATH:PATH=/build/bin/linux \
 -DPROJECT_BUILD_TESTS_UI=OFF \
 -DCMAKE_DISABLE_PRECOMPILE_HEADERS=OFF \
 -DCMAKE_VERBOSE_MAKEFILE=OFF \
 -DCMAKE_UNITY_BUILD=ON \
 -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF

RUN cmake --build /build/build/linux --config RelWithDebInfo --target dist-installer -- -j 8

USER builder
ENTRYPOINT ["/bin/bash"]
