/* Test the real hddump transfer routine against a bounded controller fixture. */
static int phase,command,transferred,polls;
char sector[512];
inb(port)
{
 polls++;
 if(phase==0 || (phase==2 && transferred))return ST_BSY;
 if(!command)return ST_DRDY;
 if(phase==1)return ST_DRDY;
 if(phase==3 || (phase==4 && transferred))return ST_DRDY|ST_ERR;
 return transferred?ST_DRDY:ST_DRDY|ST_DRQ;
}
outb(port,value) {if(port==HD_CMD)command=value;}
insw(port,data,words) char *data;
{if(port!=HD_DATA || words!=256)exit(1);transferred=1;}
outsw(port,data,words) char *data;
{if(port!=HD_DATA || words!=256)exit(1);transferred=1;}
main()
{
 int result,writing;
 for(phase=0;phase<=5;phase++)for(writing=0;writing<=1;writing++) {
  command=transferred=polls=0;
  result=hddump(makedev(1,1),1234L,sector,writing);
  if(result!=(phase<3?-1:(phase<5?0:1)) || polls>30002)exit(2);
  if(phase && command!=(writing?CMD_WRITE:CMD_READ))exit(3);
 }
 if(hddump(makedev(1,2),0L,sector,1)!=0 ||
    hddump(makedev(1,0),65536L,sector,1)!=0)exit(4);
 write(1,"dump polling: passed\n",21);return 0;
}
