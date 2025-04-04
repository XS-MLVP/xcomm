package com.xspcomm

import java.math.BigInteger
import scala.languageFeature.implicitConversions
import scala.collection.mutable.ArrayBuffer

object Main{
    def main(args: Array[String]): Unit = {
        xspcomm.init()
        println("Hello World! xpscomm version:" + Util.version())
    }
}

implicit class XPortExtension(a: XPort){
    def apply(name: String): XData = {
        a.Get(name)
    }
}

implicit class XDataExtension(a: XData){
    def :=(value: Int): Unit = {
        a.Set(value)
    }
    def :=(value: Long): Unit = {
        a.Set(value)
    }
    def :=(value: String): Unit = {
        a.Set(value)
    }
    def :=(value: Array[Byte]): Unit = {
        a.Set(value)
    }
    def :=(value: BigInteger): Unit = {
        a.Set(value)
    }
}

object XClockGlobalPtrs {
    var _CbXClockEvalList: ArrayBuffer[CbXClockEval] = ArrayBuffer[CbXClockEval]()
    var _CbXClockStepList: ArrayBuffer[CbXClockStep] = ArrayBuffer[CbXClockStep]()
}

implicit def to_cb_void_u64_voidp(cb: (Long) => Unit): cb_void_u64_voidp = {
    XClockGlobalPtrs._CbXClockStepList += new CbXClockStep(cb)
    XClockGlobalPtrs._CbXClockStepList.last
}
implicit def to_cb_int_bool(cb: (Boolean) => Unit): cb_int_bool = {
    XClockGlobalPtrs._CbXClockEvalList += new CbXClockEval(cb)
    XClockGlobalPtrs._CbXClockEvalList.last
}