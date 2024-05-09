package xspcomm

import com.xspcomm._


object Main{
    def main(args: Array[String]): Unit = {
        xspcomm.init();
        println("Hello World! xpscomm version:" + Util.version())
    }
}

// Wrapper java xpscomm class here
// TBD
