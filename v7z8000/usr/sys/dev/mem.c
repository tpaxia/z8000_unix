/* V7 memory-device minor 2: EOF/rathole. Physical and kernel memory minors
 * require machine-specific access policy and are not provided here.
 */
#include "../h/param.h"
#include "../h/dir.h"
#include "../h/user.h"

mmopen(dev)
{
	if (minor(dev) != 2)
		u.u_error = ENXIO;
}

mmread(dev)
{
	if (minor(dev) == 2)
		return;
	u.u_error = ENXIO;
}

mmwrite(dev)
{
	if (minor(dev) == 2) {
		u.u_count = 0;
		return;
	}
	u.u_error = ENXIO;
}
