CMAKE_COMMON_FLAGS ?= 
CMAKE_DEBUG_FLAGS ?= 
CMAKE_RELEASE_FLAGS ?=
CMAKE_OS_FLAGS ?= 

.PHONY: all
all: build_release/Makefile

# Debug cmake configuration
build_debug/Makefile:
	@mkdir -p build_debug
	@cd build_debug && \
      cmake -DCMAKE_BUILD_TYPE=Debug $(CMAKE_COMMON_FLAGS) $(CMAKE_DEBUG_FLAGS) $(CMAKE_OS_FLAGS) $(CMAKE_OPTIONS) ..

# Release cmake configuration
build_release/Makefile:
	@mkdir -p build_release
	@cd build_release && \
      cmake -DCMAKE_BUILD_TYPE=Release $(CMAKE_COMMON_FLAGS) $(CMAKE_RELEASE_FLAGS) $(CMAKE_OS_FLAGS) $(CMAKE_OPTIONS) ..
	@cmake --build .

.PHONY: dist-clean
dist-clean:
	@rm -rf build

.PHONY: build
build:
	@mkdir -p build
	@cd build && \
	  cmake -DCMAKE_BUILD_TYPE=Release ..
	@cmake --build .


