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

  public BigInteger S() {
    int bitLength = (int)this.W();
    if(bitLength <= 64){
      return BigInteger.valueOf(this.S64());
    }
    byte[] byteArray = this.GetBytes();
    int sign_index = (bitLength - 1) / 8;
    int sign_offst = (bitLength - 1) % 8;
    if((byteArray[sign_index] & ((byte)1 << sign_offst)) != (byte)0){
      // Negative value, assign sign
      byteArray[sign_index] |= ~(((byte)1 << (sign_offst + 1)) - 1);
      for(int i = sign_index; i < byteArray.length; i++){
        byteArray[i] = (byte)0xff;
      }
    }
    return new BigInteger(reverseEndian(byteArray));
  }

  // Get must return positive value
  public BigInteger Get() {
    int bitLength = (int)this.W();
    if(bitLength <= 64){
      return this.U64();
    }
    byte[] byteArray = this.GetBytes();
    return new BigInteger(1, reverseEndian(byteArray));
  }

  public BigInteger U() {
    return this.Get();
  }

  public void Set(byte[] v) {    
    this.SetBytes(v);
  }

  void SetBytes(byte[] v){
    UCharVector uCharVector = new UCharVector(v);
    this.SetBytesV(uCharVector);
  }

  public void Set(BigInteger v) {
    int byteLength = ((int)this.W() + 7 )/8;
    byte[] byteArray = reverseEndian(v.toByteArray());
    if(v.compareTo(BigInteger.ZERO) < 0){
      byte[] newArray = new byte[byteLength];
      for(int i = 0; i < newArray.length; i++) {
        newArray[i] = (byte)0xff;
      }
      System.arraycopy(byteArray, 0, newArray, 0, byteArray.length);
      byteArray = newArray;
    }
    this.Set(byteArray);
  }

  public void Set(int data) {
    if(data < 0){
      this.Set(BigInteger.valueOf(data));
    }else{
      this.Seti(data);
    }
  }

  public void Set(long data) {
    if(data < 0){
      this.Set(BigInteger.valueOf(data));
    }else{
      this.Setl(data);
    }
  }

%}

%apply std::vector<unsigned char> { const std::vector<unsigned char> & };

%template(UCharVector) std::vector<unsigned char>;

%ignore xspcomm::XData::Set(int64_t);
%ignore xspcomm::XData::Set(uint64_t);
%rename(Seti) xspcomm::XData::Set(int);
%rename(Setl) xspcomm::XData::Set(unsigned int);
%rename(S64) xspcomm::XData::S;
%rename(U64) xspcomm::XData::U;
%rename(SetBytesV) xspcomm::XData::SetBytes;

%include ../xcomm.i


%constant void (*DPI_TEST_LR)(void *v) = xspcomm::TEST_DPI_LR;
%constant void (*DPI_TEST_LW)(const unsigned char v) = xspcomm::TEST_DPI_LW;
%constant void (*DPI_TEST_VR)(void *v) = xspcomm::TEST_DPI_VR;
%constant void (*DPI_TEST_VW)(const void *v) = xspcomm::TEST_DPI_VW;
