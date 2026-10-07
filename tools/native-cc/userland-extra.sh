PATH=/bin
export PATH || exit 1
cd /tmp || exit 1
basename /usr/src/example.c .c > base.out || exit 1
comm left right > comm.out || exit 1
comm -12 left right > common.out || exit 1
join join1 join2 > join.out || exit 1
tr a-z A-Z < input > upper.out || exit 1
tr -d a < input > delete.out || exit 1
tr -s abc abc < squeeze > squeeze.out || exit 1
rev input > reverse.out || exit 1
split -2 input part || exit 1
cat partaa partab > reunited || exit 1
cmp input reunited || exit 1
dd if=input of=dd.out bs=3 || exit 1
cmp input dd.out || exit 1
echo -n bana > four || exit 1
dd if=four of=swap.out bs=4 conv=swab || exit 1
normal /bin/od -b four > od.out || exit 1
sum input > sum.out || exit 1
pr -t -l4 input > pr.out || exit 1
normal /bin/touch touched input || exit 1
/bin/test -f touched || exit 1
mkdir dudir || exit 1
cp input dudir/data || exit 1
ln dudir/data dudir/link || exit 1
du -s dudir > du.out || exit 1
nice echo nice-ok > nice.out || exit 1
time echo time-ok > time.out 2> time.err || exit 1
yes | dd bs=2 count=3 > yes.out 2> yes.err
status=$?
/bin/test $status -eq 0 -o $status -eq 141 || exit 1
cal 1 1970 > cal.out || exit 1
normal /bin/look app sorted > look.out || exit 1
normal /bin/tsort edges > tsort.out || exit 1
fgrep -f patterns input > fgrep.out || exit 1
fgrep -v -f patterns input > invert.out || exit 1
if fgrep absent input; then exit 1; fi
if nice /bin/missing; then exit 1; fi
if time /bin/test 1 -eq 2; then exit 1; fi
if split /tmp/missing; then exit 1; fi
if normal /bin/missing; then exit 1; fi
if normal /bin/sh -c 'kill -9 $$'; then exit 1; fi
sync || exit 1
echo USERLAND EXTRA OK || exit 1
