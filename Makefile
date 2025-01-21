RUN_EXT=run
ifeq ($(OS), Windows_NT)
	RUN_EXT = exe
endif

%.spv: %.comp
	glslc $< -o $@

%.spv: %.rgen
	glslc $< --target-spv=spv1.4 -o $@

%.spv: %.rint
	glslc $< --target-spv=spv1.4 -o $@

%.spv: %.rahit
	glslc $< --target-spv=spv1.4 -o $@

%.spv: %.rchit
	glslc $< --target-spv=spv1.4 -o $@

%.spv: %.rmiss
	glslc $< --target-spv=spv1.4 -o $@

%.spv: %.rcall
	glslc $< --target-spv=spv1.4 -o $@

rt: rt.cpp shaders/gen.spv shaders/chit.spv shaders/miss.spv
	$(CXX) -std=c++20 -lvulkan volk/volk.c -lglfw3 rt.cpp -o rt.exe
