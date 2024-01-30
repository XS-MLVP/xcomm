PYTHON_INCLUDE:=$(shell bash -c "python3-config --includes|awk '{print "$$"1}'|sed 's/-I//g'")

all:
	PYTHON_INCLUDE=$(PYTHON_INCLUDE) XSP_COMM_INCLUDE=../../include make -f Makefile.vcs all

clean:
	PYTHON_INCLUDE=$(PYTHON_INCLUDE) XSP_COMM_INCLUDE=../../include make -f Makefile.vcs clean

run:
	PYTHON_INCLUDE=$(PYTHON_INCLUDE) XSP_COMM_INCLUDE=../../include make -f Makefile.vcs run
