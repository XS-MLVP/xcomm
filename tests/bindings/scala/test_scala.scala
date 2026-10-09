
import com.xspcomm._

object LoadXSP {
  def main(args: Array[String]): Unit = {
    xspcomm.init()
    println("Hello World! xpscomm version:" + Util.version())
    var x1 = new XData(32, IOType.Input)
    var port = new XPort("port1")
    println(x1.W())
    x1.Set(0x123123)
    println(x1.AsBinaryString())

    x1 := 0x123123
    println(x1.AsBinaryString())

    x1 := "0x2223334448899"
    println(x1.AsBinaryString())

    var clock = new XClock((dump: Boolean) => {
      println("Clock callback: " + dump)
    })

    clock.StepRis((cycle: Long) => {
      println("Rising edge callback: " + cycle)
    })
    port.Add("pin1", x1)
    println("xdata = " + port("pin1").Get())
    clock.Step(5)

    // test xclock u64 step/cbs
    var clk = new XClock(Util.TEST_get_u64_step_func(), new java.math.BigInteger("123"))
    clk.StepRis(Util.TEST_get_u64_ris_fal_cblback_func(), new java.math.BigInteger("456"))
    clk.StepFal(Util.TEST_get_u64_ris_fal_cblback_func(), new java.math.BigInteger("987"))
    clk.Step(3)
  }
}
