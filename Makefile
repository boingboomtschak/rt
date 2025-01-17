RUN_EXT=run
ifeq ($(OS), Windows_NT)
	RUN_EXT = exe
endif

%.spv: %.comp
	glslc $< -o $@

rt: rt.cpp
	$(CXX) -std=c++20 -lvulkan volk/volk.c -lglfw3 rt.cpp -o rt.exe
