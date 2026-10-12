import xspcomm as x

def test_third_call(args):
    print("test_third_call called with args: ", args)
    return [1,2]


def test_a(a):
    print(a)

if __name__ == "__main__":
    if x.init_third_call():
        print("inited init_third_call complete")
        x.test_third_call()
    else:
        print("init_third_call failed")
