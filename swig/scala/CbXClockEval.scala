package com.xspcomm;
import _com.xspcomm._;

class CbXClockEval(cb: (Boolean) => Unit) extends cb_int_bool {
  var callback: (Boolean) => Unit = cb
  this.set_force_callable()
  
  override def call(dump: Boolean): Int = {
    if (this.callback != null) {
      this.callback(dump)
    }else{
      println("Fake eval, dump = " + dump + ", need a callback when new CbXClockEval");
    }
    0
  }
}
