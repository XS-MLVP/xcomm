%module(directors="1") Util

/* 兼容std::vector<unsigned char> */
%include <std_vector.i>
%typemap(jni) std::vector<unsigned char> "jbyteArray"
%typemap(jtype) std::vector<unsigned char> "byte[]"
%typemap(jstype) std::vector<unsigned char> "byte[]"
%typemap(javain) std::vector<unsigned char> "$javainput"
%typemap(javaout) std::vector<unsigned char> { return $jnicall; }

%typemap(in) std::vector<unsigned char> (std::vector<unsigned char> vec) {
  if (!$input) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null array");
    return $null;
  }
  const jsize sz = jenv->GetArrayLength($input);
  jbyte* const jarr = jenv->GetByteArrayElements($input, 0);
  if (!jarr) return $null;
  vec.assign(jarr, jarr+sz);
  jenv->ReleaseByteArrayElements($input, jarr, JNI_ABORT);
  $1 = &vec;
}

%typemap(out) std::vector<unsigned char> {
  const jsize sz = $1.size();
  $result = jenv->NewByteArray(sz);
  jenv->SetByteArrayRegion($result, 0, sz, reinterpret_cast<signed char *>($1.data()));
}

%apply std::vector<unsigned char> { const std::vector<unsigned char> & };

%template(UCharVector) std::vector<unsigned char>;

/* 增加char**argv与java数组映射规则*/
%apply char **STRING_ARRAY { char ** };

%include ../xcomm.i

