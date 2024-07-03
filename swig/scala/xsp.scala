package com.xspcomm

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

implicit def to_cb_void_u64_voidp(cb: (Long) => Unit): cb_void_u64_voidp = {new CbXClockStep(cb)}
implicit def to_cb_int_bool(cb: (Boolean) => Unit): cb_int_bool = {new CbXClockEval(cb)}
