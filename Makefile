.DEFAULT_GOAL = rbuild

build:
	cmake . -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_BUILD_PARALLEL=`nproc`
	cd build && make -j`nproc`

install: clean build
	cd build && make install

clean:
	rm -rf build dist

test_lua: build
	LUA_PATH=build/lua/?.lua lua tests/test_lua.lua

ifeq ($(BUILD_XSPCOMM_VCS_UVMPS),)
test_python: build
	PYTHONPATH=build/python python3 tests/test_python.py

test_java: build
	java -cp build/java/xspcomm-java.jar tests/test_java.java

test_scala: build
	scala -cp build/scala/xspcomm-scala.jar tests/test_scala.scala

test_golang: build
	GO111MODULE=off GOPATH="`pwd`/build/golang" go run tests/test_golang.go

else
test_python: build
	LD_PRELOAD=build/lib/libuvmps_vcs.so PYTHONPATH=build/python python3 tests/test_python.py

test_java: build
	LD_PRELOAD=build/lib/libuvmps_vcs.so java -cp build/java/xspcomm-java.jar tests/test_java.java

test_scala: build
	LD_PRELOAD=build/lib/libuvmps_vcs.so scala -cp build/scala/xspcomm-scala.jar tests/test_scala.scala

test_golang: build
	LD_PRELOAD=build/lib/libuvmps_vcs.so GO111MODULE=off GOPATH="`pwd`/build/golang" go run tests/test_golang.go

endif

test_pythird_call: build
	cd tests && (PYTHONPATH=../build/python PYTHON_THIRD_MODUEL=test_pythirdcall python3 test_pythirdcall.py)


run: build
	./build/tests/test_xdata

rbuild: clean build run

wheel: clean
	XSPCOMM_INSTALL_PREFIX=picker/ XSPCOMM_BUILD_WHEEL=1 pipx run build

build_all:
	make BUILD_XSPCOMM_SWIG=python,scala,java,golang,lua

test_all: build_all test_lua test_python test_java test_scala test_golang test_pythird_call
	@echo "All tests passed"
