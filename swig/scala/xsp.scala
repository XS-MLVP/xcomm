package com

import java.math.BigInteger
import scala.languageFeature.implicitConversions
import scala.collection.mutable.ArrayBuffer

object Main{
    def main(args: Array[String]): Unit = {
        xspcomm.xspcommLoader.init()
        println("Hello World! xpscomm version:" + xspcomm.Util.version())
    }
}

package object xspcomm {

    object xspcomm{
        def init(): Unit = {
            xspcommLoader.init()
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

    implicit def to_cb_void_u64_voidp(cb: (Long) => Unit): cb_void_u64_voidp = {
        XClockGlobalPtrs._CbXClockStepList += new CbXClockStep(cb)
        XClockGlobalPtrs._CbXClockStepList.last
    }

    implicit def to_cb_int_bool(cb: (Boolean) => Unit): cb_int_bool = {
        XClockGlobalPtrs._CbXClockEvalList += new CbXClockEval(cb)
        XClockGlobalPtrs._CbXClockEvalList.last
    }

    object XClockGlobalPtrs {
        var _CbXClockEvalList: ArrayBuffer[CbXClockEval] = ArrayBuffer[CbXClockEval]()
        var _CbXClockStepList: ArrayBuffer[CbXClockStep] = ArrayBuffer[CbXClockStep]()
    }

    trait BaseDUTTrait{
        def GetXClock(): XClock
        def GetXPort(): XPort
        def OpenWaveform(): Boolean
        def CloseWaveform(): Boolean
        def SetWaveform(wave_name: String): Unit
        def FlushWaveform(): Unit
        def SetCoverage(coverage_name: String): Unit
        def Step(i: Int = 1): Unit
        def StepRis(callback: (Long) => Unit): Unit
        def StepFal(callback: (Long) => Unit): Unit
        def Finish(): Unit
        def InitClock(clock_name: String): Unit
        def RefreshComb(): Unit
        def CheckPoint(check_point: String): Unit
        def Restore(check_point: String): Unit
        def VPIInternalSignalList(prefix: String = "", deep: Int = 99): Array[String]
        def GetInternalSignal(name: String, index: Int = -1, use_vpi: Boolean = false): XData
        def GetInternalSignal(name: String, is_array: Boolean): Array[XData]
        def GetInternalSignalList(prefix: String="", deep: Int = 99, use_vpi: Boolean = false): Array[String]
        def apply(key: String): XData = {
            this.GetXPort().Get(key)
        }
    }
}
