package com.xspcomm;

class CbXClockStep(cb: (Long) => Unit) extends cb_void_u64_voidp {
  var callback: (Long) => Unit = cb
  this.set_force_callable()
  
  override def call(cycle: java.math.BigInteger, p: SWIGTYPE_p_void): Unit = {
    if (this.callback != null) {
      this.callback(cycle.longValue())
    }else{
        println("Empty Step call with cycle: " + cycle + " , need a callback when new CbXClockStep");
    }
  }
}
