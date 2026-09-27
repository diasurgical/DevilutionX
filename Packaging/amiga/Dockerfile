FROM amigadev/crosstools:m68k-amigaos-gcc10

RUN apt-get update && apt-get install --no-install-recommends -y smpq

CMD cmake -S. -Bbuild-amiga -DCPACK=ON \
		-DCMAKE_BUILD_TYPE=Release \
		-DM68K_CPU=68040 \
		-DM68K_FPU=hard \
		-DM68K_COMMON="-s -fbbb=- -ffast-math" && \
	cmake --build build-amiga -j $(nproc)
