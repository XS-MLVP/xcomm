import tlm_pbsb as u

def test_msg():
    print("")
    print(" test python 2 python, uvm 2 python, python 2 uvm")
    print("==================Topology===================")
    print("     UVM Side     Channel      Python  Side  ")
    print("                    |       +-->  receive 0  ")
    print("      prod   ------foo------+-->  receive 1  ")
    print("                    |                        ")
    print("      cons  <------bar------ pub1            ")
    print("                    |                        ")
    print("                    |         --> receive 2  ")
    print("                    |        |               ")
    print("                    |        <--  sub2       ")
    print("==================Topology===================")

    msg = u.tlm_msg()
    receive0 = u.TLMSub("foo", lambda a: print("receive 0 >>>>", a.as_bytes()))
    receive1 = u.TLMSub("foo", lambda a: print("receive 1 >>>>", a.as_bytes()))
    receive0.Connect()
    receive1.Connect()

    pub = u.TLMPub("test")
    pub.Connect()
    receive2 = u.TLMSub("test", lambda a: print("receive 2 >>>>", a.as_bytes()))
    receive2.Connect()
 
    pub1 = u.TLMPub("bar")
    pub1.Connect()

    u.tlm_vcs_init("_tlm_pbsb.so", "-no_save")

    for i in range(20):
        # step uvm + systemc
        u.tlm_pbsb_run(1)
        u.tlm_vcs_step(1)
        # send message
        msg.from_bytes(bytes(str(i).encode()))
        pub.SendMsg(msg)
        pub1.SendMsg(msg)

if __name__ == "__main__":
    test_msg()

