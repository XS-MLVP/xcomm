
from xspcomm import *

def test_exception():
    def cb_ris(c):
        assert 0
    clk = XClock(lambda a: 1 if print("lambda stp: ", a) else 0)
    clk.StepRis(cb_ris)
    clk.Step(3)
    print(clk)


if __name__ == "__main__":
    test_exception()