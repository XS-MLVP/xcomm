import com.xspcomm.*;


import java.math.BigInteger;

public class test_java {

  public static void main(String argv[]) {
    xspcomm.init();
    XData a = new XData(32, XData.In);
    XData b = new XData(32, XData.In);
    XData c = new XData(128, XData.In);
    XData e = new XData(0, XData.In);
    long l_value = 0xfdfdfd56;
    a.Set(l_value);
    Util.TEST_DPI_LW((short)0);
    System.out.println("----------------------------");
    b.SetWriteMode(b.Imme);
    e.SetWriteMode(e.Imme);
    b.BindDPIRW(UtilConstants.DPI_TEST_VR, UtilConstants.DPI_TEST_VW);
    e.BindDPIRW(UtilConstants.DPI_TEST_LR, UtilConstants.DPI_TEST_LW);
    b.Set(0x12345678);
    e.Set(1);
    e.Set(0);
    BigInteger bi = new BigInteger("481237234234234234");
    c.Set(bi);
    System.out.println("Bigint input data  = " + bi.toString());
    System.out.println("Bigint output data = " + c.Get().toString());

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
    
    CbXClockEval cb = new CbXClockEval();
    XClock clock = new XClock(cb);
    clock.StepRis(new CbXClockStep());

    clock.Step(5);
    System.out.println("XClock: "+clock);
    System.out.println("Test complete!");
  }
}
