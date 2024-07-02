package com.xspcomm;
import java.util.function.Consumer;


public class CbXClockStep extends cb_void_u64_voidp {
  private Consumer<Long> callback;
  
  public CbXClockStep(Consumer<Long> callback) {
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
      this.callback.accept(cycle.longValue());
    }else{
        System.out.println("Empty Step call with cycle: " + cycle + " , need a callback when new CbXClockStep");
    }
  }
}
