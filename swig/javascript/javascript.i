%module(directors="1") pyxspcomm

%{
#include "extend.h"
%}

%typemap(in) Napi::Function {
  $1 = info[0].As<Napi::Function>();
}

%ignore xspcomm::XClock;
%rename (XClock) JSXClock;

%include ../xcomm.i
%include "extend.h"
