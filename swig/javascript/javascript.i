%module(directors="1") pyxspcomm

%typemap(in) std::vector<unsigned char>& (std::vector<unsigned char> temp) {
    if (!info[0].IsBigInt()) {
        Napi::TypeError::New(env, "Expected a BigInt").ThrowAsJavaScriptException();
    }
    // Get the BigInt value
    Napi::BigInt bigInt = info[0].As<Napi::BigInt>();
    // Get the byte representation of the BigInt
    size_t wordCount = bigInt.WordCount();
    std::vector<uint64_t> words(wordCount);
    int sign_bit;
    bigInt.ToWords(&sign_bit, &wordCount, words.data());
    // Convert the words to bytes and store them in a std::vector<unsigned char>
    for (uint64_t word : words) {
        auto byte = (unsigned char *)&word;
        for (int i = 0; i < 8; ++i) {
            temp2.push_back(byte[i]);
        }
    }
    $1 = &temp2;   
}

%typemap(out) std::vector<unsigned char> {
    // Convert the std::vector<unsigned char> to words
    std::vector<unsigned char> *vec = &$1;
    auto wordCount = vec->size() / 8;
    if (vec->size() % 8 != 0) {
        wordCount += 1;
    }
    std::vector<uint64_t> words(wordCount);
    words[wordCount-1] = 0;
    
    auto buffer = (unsigned char *)words.data();
    for (int i = 0; i < vec->size(); ++i) {
        buffer[i] = vec->at(i);
    }
    Napi::BigInt bigInt = Napi::BigInt::New(env, 0, words.size(), words.data());
    $result = bigInt;
}

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
