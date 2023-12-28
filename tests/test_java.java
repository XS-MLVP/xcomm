public class test_java {
  static {
    System.loadLibrary("javaxspcomm");
  }

  // example example = new example();
  public static void main(String argv[]) {

    XData a = new XData(32, XData.In);
    XData b = new XData(32, XData.In);
    XData c = new XData(128, XData.In);
    long l_value = 0xfdfdfd56;
    a.Set(l_value);
    b.Set(0x12345678);

    byte[] byttest = a.GetVU8();
    System.out.println("a(hex) = " + a.U().toString(16));

    StringBuilder sb = new StringBuilder();
    for (byte xb : byttest) {
      sb.append(String.format("%02x", xb));
    }
    System.out.println("a byte = " + sb.toString());

    System.out.println("b = " + b.GetVU8());
    System.out.println("b = " + b.U());
    System.out.println("b = " + b.U().toString(16));

    byte[] byttest2 = { 0x01, 0x02, 0x03, 0x04 };
    UCharVector ucv = new UCharVector(byttest2);
    c.SetVU8(ucv);
    System.out.println("c = " + c.GetVU8());
    System.out.println("c = " + c.U());
    System.out.println("c = " + c.U().toString(16));

    XPort port = new XPort("x_");
    port.Add("a", a);
    port.Add("b", b);
    System.out.println(port);
    System.out.println("XData: " + a + "  :  port get a: " + port.Get("a"));
    System.out.println("XData: " + b + "  :  port get b: " + port.Get("b"));
    
    XClockCbStep cb = new XClockCbStep();
    cb.set_force_callable();
    cb.call(false);
    XClock clock = new XClock(cb);
    clock.Step();
    System.out.println("XClock: "+clock);
    
  }
}