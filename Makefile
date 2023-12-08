.DEFAULT_GOAL = rbuild

build:
	cmake . -Bbuild
	cd build && make -j`nproc`

install: clean build
	cd build && make install

clean:
	rm -rf build dist

test_python: build
	PYTHONPATH=build/python python3 tests/test_python.py

run: build
	./build/tests/test_xdata

rbuild: clean build run
