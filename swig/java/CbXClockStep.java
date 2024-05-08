package com.xspcomm;
import java.util.function.Consumer;


public class CbXClockStep extends cb_void_u64_voidp {
  private Consumer<java.math.BigInteger> callback;
  
  public CbXClockStep(Consumer<java.math.BigInteger> callback) {
    this.callback = callback;
    this.set_force_callable();
  }

  public CbXClockStep() {
    this.callback = null;
    this.set_force_callable();
  }

  @Override
  public void call(java.math.BigInteger cycle, SWIGTYPE_p_void __void) {
    if (this.callback != null) {
      this.callback.accept(cycle);
    }else{
        System.out.println("Empty Step call with cycle: " + cycle + " , need a callback when new CbXClockStep");
    }
  }
}
