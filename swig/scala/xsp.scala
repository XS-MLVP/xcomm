package com.xspcomm

import _com.xspcomm.{XPort => JXPort}
import _com.xspcomm.{XClock => JXClock}
import _com.xspcomm.{Util => JUtil}
import _com.xspcomm.{XData => JXData}
import _com.xspcomm.{xspcomm => jxspcomm}


object Main{
    def main(args: Array[String]): Unit = {
        jxspcomm.init();
        println("Hello World! xpscomm version:" + JUtil.version())
    }
}

object xspcomm{
    def init(): Unit = {
        jxspcomm.init();
    }
}

object Util{
    // export static methods from java Util class
    def version(): String = {
        return JUtil.version();
    }
    // ...
}

// Wrapper java xpscomm class here
class XPort extends JXPort {
    // TBD
}

class XClock extends JXClock {
    // TBD
}

class XData extends JXData {
    // TBD
}
