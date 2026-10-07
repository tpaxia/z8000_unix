/* Appended to the actual core() policy: credentials cannot be manufactured
 * through setuid(), which deliberately changes both real and effective IDs.
 */
struct user u;
struct inode node;
int exists, looked, made, truncated, written, released, denied, ioerror, failed;
struct inode *namei(f,mode) int (*f)();
{ looked++;return(exists ? &node : 0); }
struct inode *maknode(mode)
{ made=mode;return(&node); }
schar() { return(0); }
access(ip,mode) struct inode *ip;
{ if(denied) u.u_error=denied;return(denied!=0); }
itrunc(ip) struct inode *ip; { truncated++; }
coredump(ip) struct inode *ip; { written++;u.u_error=ioerror; }
iput(ip) struct inode *ip; { released++; }
check(s,ok) char *s;
{ if(!ok){printf("corepolicy: FAIL %s\n",s);failed++;} }
reset()
{
 u.u_uid=u.u_ruid=1;u.u_gid=u.u_rgid=2;
 node.i_mode=0100666;exists=1;
 looked=made=truncated=written=released=denied=ioerror=0;
}
main()
{
 reset();u.u_uid=0;
 check("setuid refusal before lookup",!core() && !looked && !made && !written);
 reset();u.u_gid=0;
 check("setgid refusal before lookup",!core() && !looked && !made && !written);
 reset();denied=EROFS;
 check("read-only filesystem",!core() && !truncated && !written && released==1 && u.u_error==EROFS);
 reset();node.i_mode=040777;
 check("nonregular refusal",!core() && !truncated && !written && released==1);
 reset();exists=0;
 check("V7 creation mode",core() && made==0666 && truncated==1 && written==1 && released==1);
 reset();ioerror=ENOSPC;
 check("incomplete image",!core() && released==1 && u.u_error==ENOSPC);
 printf("corepolicy: %s\n",failed?"FAILED":"passed");return(failed!=0);
}
