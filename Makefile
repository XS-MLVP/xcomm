.DEFAULT_GOAL = rbuild

build:
	cmake . -Bbuild
	cd build && make -j`nproc`

install: clean build
	cd build && make install

clean:
	rm -rf build dist

ifeq ($(BUILD_XSPCOMM_VCS_UVMPS),)
test_python: build
	PYTHONPATH=build/python python3 tests/test_python.py
else
test_python: build
	LD_PRELOAD=build/lib/libuvmps_vcs.so PYTHONPATH=build/python python3 tests/test_python.py
endif

test_pythird_call: build
	cd tests && (PYTHONPATH=../build/python PYTHON_THIRD_MODUEL=test_pythirdcall python3 test_pythirdcall.py)


run: build
	./build/tests/test_xdata

rbuild: clean build run
