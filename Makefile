CXX ?= clang++

.PHONY: release debug lldb
release:
	mkdir -p build && \
    cd build       && \
    conan install .. --profile ../profiles/release && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build .

debug: lldb
	echo launching raw

clean:
	rm -rf build

lldb:
	$(CXX) *.cpp -glldb -o lldb -std=c++17 -fcoroutines-ts -lboost_program_options -lboost_system
