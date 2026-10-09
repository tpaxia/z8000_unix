/* V7 memory special file: physical memory, kernel data, EOF/rathole.
 * Address translation belongs to the selected machine's membyte().
 */
#include "../h/param.h"
#include "../h/dir.h"
#include "../h/user.h"

mmopen(dev)
{
 if(minor(dev)>2) u.u_error=ENXIO;
 else if(minor(dev)!=2) suser();
}

mmread(dev)
{
 char c;
 if(minor(dev)==2) return;
 while(u.u_count && !u.u_error) {
  if(membyte(u.u_offset, minor(dev)==1, &c, 0)<0) {
   u.u_error=ENXIO; break;
  }
  if(passc(c&0377)<0) break;
 }
}

mmwrite(dev)
{
 int c;
 char value;
 if(minor(dev)==2) {u.u_count=0; return;}
 while(u.u_count && !u.u_error) {
  c=cpass();
  if(c<0 || u.u_error) break;
  value=c;
  if(membyte(u.u_offset-1, minor(dev)==1, &value, 1)<0) {
   u.u_error=ENXIO; break;
  }
 }
}
