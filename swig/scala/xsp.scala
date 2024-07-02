package com.xspcomm

import _com.xspcomm.{XPort => JXPort}
import _com.xspcomm.{XClock => JXClock}
import _com.xspcomm.{Util => JUtil}
import _com.xspcomm.{XData => JXData}
import _com.xspcomm.{xspcomm => jxspcomm}
import _com.xspcomm.{IOType => JIOType}


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

object IOType {
    var Input = JIOType.Input
    var Output = JIOType.Output
    var InOut = JIOType.InOut
}

// Wrapper java xpscomm class here
class XPort extends JXPort {
    // TBD
}

class XClock(callback: (Boolean) => Unit) extends JXClock(new CbXClockEval(callback)) {

    def StepRis(cb: (cycle: Long) => Unit): Unit = {
        this.StepRis(new CbXClockStep(cb))
    }

    def StepFal(cb: (cycle: Long) => Unit): Unit = {
        this.StepFal(new CbXClockStep(cb))
    }
}

class XData(width:Long, itype:JIOType, name: String = "") extends JXData(width, itype, name) {}
