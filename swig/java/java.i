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

%typemap(javaimports)xspcomm::XData %{
import java.math.BigInteger;
import java.util.ArrayList;
import java.util.List;
%}

%typemap(javacode) xspcomm::XData %{
  private static byte[] reverseEndian(byte[] bigEndianBytes) {
    byte[] littleEndianBytes = new byte[bigEndianBytes.length];
    for (int i = 0; i < bigEndianBytes.length; i++) {
      littleEndianBytes[i] = bigEndianBytes[bigEndianBytes.length - 1 - i];
    }
    return littleEndianBytes;
  }

  public BigInteger Get() {
    byte[] byteArray = reverseEndian(this.GetVU8());
    return new BigInteger(1, byteArray);
  }

  public void Set(BigInteger v) {
    UCharVector uCharVector = new UCharVector(reverseEndian(v.toByteArray()));
    this.SetVU8(uCharVector);
  }
%}

%apply std::vector<unsigned char> { const std::vector<unsigned char> & };

%template(UCharVector) std::vector<unsigned char>;

%ignore xspcomm::XData::Set(int64_t);
%ignore xspcomm::XData::Set(uint64_t);

%include ../xcomm.i


%constant void (*DPI_TEST_LR)(void *v) = xspcomm::TEST_DPI_LR;
%constant void (*DPI_TEST_LW)(const unsigned char v) = xspcomm::TEST_DPI_LW;
%constant void (*DPI_TEST_VR)(void *v) = xspcomm::TEST_DPI_VR;
%constant void (*DPI_TEST_VW)(const void *v) = xspcomm::TEST_DPI_VW;
