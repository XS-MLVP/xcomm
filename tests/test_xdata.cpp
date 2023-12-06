#include "xspcomm/xcomm.h"
#include "xspcomm/xinstance.h"

using namespace xspcomm;

int main(int argsc, const char **argsv)
{
    Debug("version: %s", version().c_str());
    checkVersion();
    test_xdata();
    return 0;
}
