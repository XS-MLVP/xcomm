package main

import (
	"fmt"
	"math/big"
	"xspcomm"
)

func step_fc1(b bool) int {
	fmt.Println("step1:", b)
	return 0
}

func step_fc2(b bool) int {
	fmt.Println("step2:", b)
	return 0
}

func step_cb(c uint64){
	fmt.Println("step:", c)
}

func main() {
	fmt.Println("xpscomm version:", xspcomm.Version())
	// test XData
	x1 := xspcomm.NewXData(129, xspcomm.IOType_Input)
	x1.Set(123)
	fmt.Println("x1 = ", x1.S64(), " u=", x1.U64())
	v := new(big.Int).SetInt64(-1236)
	x1.Set(v)
	fmt.Println("x1 = ", x1.Get(), " [big.Int] ", v, " S:", x1.S(), " S64:", x1.S64())
	// test XClock
	xclock := xspcomm.NewXClock(step_fc1)
	xclock.StepRis(step_cb)
	xclock.Step(3)
	xclock.ReInit(step_fc2)
	xclock.Step(2)
}
