public class XClockCbStep extends cb_int_bool {
  public int count  = 0;
  @Override
  public int call(boolean __args1) {
    this.count++;
    System.out.println("XClockCbStep callback: " + this.count);
    return 0;
  }
}