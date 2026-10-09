#include <stdio.h>
#include <signal.h>
struct result { int words[256]; };
long caught;
int bad, inhook;
/* The test's assembly instrumenter invokes this halfway through the return
 * copy. The handler re-enters make(), with the nested hook suppressed. */
copyhook()
{
 if(inhook)return;
 inhook=1;kill(getpid(),SIGALRM);inhook=0;
}
struct result make(n)
int n;
{
 struct result r;
 int i;
 for(i=0;i<256;i++)r.words[i]=n+i;
 return r;
}
handler()
{
 struct result r;
 signal(SIGALRM,handler);
 caught++;
 r=make(-1000);
 if(r.words[0]!=-1000 || r.words[255]!=-745)bad++;
 alarm(5);
}
main()
{
 struct result r;
 int n,i;
 signal(SIGALRM,handler);alarm(1);
 for(n=0;n<64;n++) {
  r=make(n);
  for(i=0;i<256;i++)if(r.words[i]!=n+i)bad++;
 }
 alarm(0);
 printf("aggregate: %ld signals, %d corrupt words\n",caught,bad);
 if(caught<64 || bad)return 1;
 puts("aggregate: passed");return 0;
}
