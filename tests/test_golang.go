package main

import (
	"fmt"
	"math/big"
	"xspcomm"
)

func main() {
	fmt.Println("xpscomm version:", xspcomm.Version())
	x1 := xspcomm.NewXData(129, xspcomm.IOType_Input)
	x1.Set(123)
	fmt.Println("x1 = ", x1.S64(), " u=", x1.U64())
	v := new(big.Int).SetInt64(-1236)
	x1.Set(v)
	fmt.Println("x1 = ", x1.Get(), " [big.Int] ", v, " S:", x1.Signed(), " S64:", x1.S64())
}
