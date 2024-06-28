package main

import (
	"fmt"
	"math/big"
	"xspcomm"
)

func main() {
	fmt.Println("xpscomm version:", xspcomm.Version())
	x1 := xspcomm.NewXData(128, xspcomm.IOType_Input)
	x1.Set(123)
	fmt.Println("x1 = ", x1.S())
	v := new(big.Int).SetInt64(1234567)
	x1.Set(v)
	fmt.Println("x1 = ", x1.AsBinaryString(), " [big.Int] ", v, " S:", x1.Get())
}
