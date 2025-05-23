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
    XData x4  = new XData(4, XData.In);
    XData x128  = new XData(129, XData.In);
    x4.Set(-1);
    x128.Set(new BigInteger("-1"));
    System.out.printf("test negative assigen x4   = %d\n", x4.S());
    System.out.printf("test negative assigen x128 = %d\n", x128.Get());
    x128.Set(-1);
    System.out.printf("test negative assigen x128 = %d (%d)\n", x128.Get(), x128.S());

    byte[] byttest = a.GetBytes();
    System.out.println("a(hex) = " + a.U64().toString(16));

    StringBuilder sb = new StringBuilder();
    for (byte xb : byttest) {
      sb.append(String.format("%02x", xb));
    }
    System.out.println("a byte = " + sb.toString());

    System.out.println("b = " + b.GetBytes());
    System.out.println("b = " + b.U64());
    System.out.println("b = " + b.U64().toString(16));

    byte[] byttest2 = { 0x01, 0x02, 0x03, 0x04 };
    UCharVector ucv = new UCharVector(byttest2);
    c.SetVU8(ucv);
    System.out.println("c = " + c.GetBytes());
    System.out.println("c = " + c.U64());
    System.out.println("c = " + c.U64().toString(16));

    XPort port = new XPort("x_");
    port.Add("a", a);
    port.Add("b", b);
    System.out.println(port);
    System.out.println("XData: " + a + "  :  port get a: " + port.Get("a"));
    System.out.println("XData: " + b + "  :  port get b: " + port.Get("b"));
    
    XClock clock = new XClock((dump)->{System.out.println("dump: "+dump);});
    clock.StepRis((cycle)->{System.out.println("Rising edge: "+cycle);});

    clock.Step(5);
    System.out.println("XClock: "+clock);
    System.out.println("Test complete!");
    XClock clk = new XClock(Util.TEST_get_u64_step_func(), new BigInteger("123"));
    clk.StepRis(Util.TEST_get_u64_ris_fal_cblback_func(), new BigInteger("456"));
    clk.StepFal(Util.TEST_get_u64_ris_fal_cblback_func(), new BigInteger("789"));
    clk.Step(3);
  }
}
