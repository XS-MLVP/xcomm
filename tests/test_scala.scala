
import com.xspcomm._

object LoadXSP {
  def main(args: Array[String]): Unit = {
    xspcomm.init()
    println("Hello World! xpscomm version:" + Util.version())
  }
}
