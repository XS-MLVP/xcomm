.DEFAULT_GOAL = rbuild

build:
	cmake . -Bbuild
	cd build && make -j`nproc`

install: build
	cd build && make install

clean:
	rm -rf build

test_python: build
	PYTHONPATH=build/lib/python python3 tests/test_python.py

run: build
	./build/tests/test_xdata

rbuild: clean build run
