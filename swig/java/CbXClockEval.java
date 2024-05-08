package com.xspcomm;
import java.util.function.Consumer;

public class CbXClockEval extends cb_int_bool {
  
  private Consumer<Boolean> callback;

  public CbXClockEval(Consumer<Boolean> callback) {
    this.callback = callback;
    this.set_force_callable();
  }

  public CbXClockEval() {
    this.callback = null;
    this.set_force_callable();
  }

  @Override
  public int call(boolean dump) {
    if (this.callback != null) {
      this.callback.accept(dump);
    }else{
      System.out.println("Fake eval, dump = " + dump + ", need a callback when new CbXClockEval");
    }
    return 0;
  }
}
