package main

import (
	"fmt"
	"math/big"
	"xspcomm"
)

func main() {
	fmt.Println("xpscomm version:", xspcomm.Version())
	x1 := xspcomm.NewXData(32, xspcomm.IOType_Input)
	x1.Set(123)
	v := new(big.Int).SetInt64(213)
	fmt.Println("x1 = ", x1.AsBinaryString())
	x1.Set(v)
	fmt.Println("x1 = ", x1.AsBinaryString(), " [big.Int] ", v)
}
