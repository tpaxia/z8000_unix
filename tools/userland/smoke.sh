PATH=/bin
export PATH || exit 1
chmod 4755 /bin/mkdir /bin/rmdir /bin/mv || exit 1
cd /tmp || exit 1
rm -rf tree || exit 1
sed 's/beta/BETA/' input > sed.out || exit 1
grep 'BETA 2' sed.out || exit 1
expr 7 + 5 > expr.out || exit 1
grep '^12$' expr.out || exit 1
egrep 'alpha|gamma' input > egrep.out || exit 1
normal /bin/diff input changed > diff.out || exit 1
grep 'beta 4' diff.out || exit 1
mkdir tree || exit 1
cp input tree/data || exit 1
find tree -type f -print > find.out || exit 1
grep 'tree/data' find.out || exit 1
tar cf archive tree || exit 1
rm tree/data || exit 1
rmdir tree || exit 1
tar xf archive || exit 1
cmp input tree/data || exit 1
echo '2 3 + p' | dc > dc.out || exit 1
grep '^5$' dc.out || exit 1
echo '2+3' | bc > bc.out || exit 1
grep '^5$' bc.out || exit 1
echo 'define(foo,bar)foo' | m4 > m4.out || exit 1
grep bar m4.out || exit 1
awk '{s += $2} END {print s}' input > awk.out || exit 1
grep '^6$' awk.out || exit 1
echo '123456789 * 987654321' | bc > big.out || exit 1
grep '^121932631112635269$' big.out || exit 1
echo 'sqrt(81)' | bc > sqrt.out || exit 1
grep '^9' sqrt.out || exit 1
echo 'Hello formatter' | nroff -Tlp > nroff.out || exit 1
grep 'Hello formatter' nroff.out || exit 1
echo '.EQ' > equation || exit 1
echo 'x sup 2' >> equation || exit 1
echo '.EN' >> equation || exit 1
normal /bin/neqn equation > neqn.out || exit 1
normal /bin/eqn equation > eqn.out || exit 1
grep '.EQ' neqn.out || exit 1
echo '.TS' > table || exit 1
echo 'l.' >> table || exit 1
echo 'tabledata' >> table || exit 1
echo '.TE' >> table || exit 1
normal /bin/tbl table > tbl.out || exit 1
grep tabledata tbl.out || exit 1
nroff -Tlp tbl.out > tabtext.out || exit 1
grep tabledata tabtext.out || exit 1
normal /bin/deroff table > deroff.out || exit 1
grep tabledata deroff.out || exit 1
echo 'main(){int a;a=1;}' | normal /bin/cb > cb.out || exit 1
grep main cb.out || exit 1
normal /bin/lcount < input > lcount.out || exit 1
grep '^3$' lcount.out || exit 1
echo apple > words || exit 1
echo banana >> words || exit 1
/usr/lib/spellin < words > dictionary || exit 1
/usr/lib/spellout dictionary < words > unknown || exit 1
test ! -s unknown || exit 1
echo '0 0' > points || exit 1
echo '1 1' >> points || exit 1
normal /bin/graph < points > graph.out || exit 1
test -s graph.out || exit 1
normal /bin/spline < points > spline.out || exit 1
test -s spline.out || exit 1
echo 'int square(n) int n; { return n*n; }' > lint.c || exit 1
lint -n lint.c > lint.out || exit 1
echo '      i=1' > f77.in || exit 1
echo '      end' >> f77.in || exit 1
struct f77.in > struct.out || exit 1
grep 'i = 1' struct.out || exit 1
echo 'answer(){return(42);} main(){return(answer()!=42);}' > obj.c || exit 1
cc -O -c obj.c || exit 1
nm obj.b > nm.out || exit 1
grep _answer nm.out || exit 1
ar rc obj.a obj.b || exit 1
cc -i runtime.c -lmp -o runtime || exit 1
./runtime || exit 1
cc -i permissions.c -o permissions || exit 1
./permissions || exit 1
cc -i dbm.c -ldbm -o dbmtest || exit 1
./dbmtest || exit 1
cc -i fortran.c -lF77 -lI77 -lm -o fortrantest || exit 1
./fortrantest || exit 1
normal /usr/games/fortune > fortune.out || exit 1
nm obj.a > nmar.out || exit 1
grep _answer nmar.out || exit 1
normal /bin/file obj.a > arfile.out || exit 1
grep archive arfile.out || exit 1
cc -i obj.b -o obj || exit 1
normal /bin/size obj > size.out || exit 1
strip obj || exit 1
./obj || exit 1
normal /bin/file obj > file.out || exit 1
grep executable file.out || exit 1
stty echo || exit 1
normal /bin/tty > tty.out || exit 1
grep /dev/console tty.out || exit 1
/usr/lib/spellin < /usr/dict/words > /usr/dict/hlist || exit 1
cat /usr/src/cmd/spell/american /usr/src/cmd/spell/local | /usr/lib/spellin /usr/dict/hlist > /usr/dict/hlista || exit 1
cat /usr/src/cmd/spell/british /usr/src/cmd/spell/local | /usr/lib/spellin /usr/dict/hlist > /usr/dict/hlistb || exit 1
/usr/lib/spellin < /usr/src/cmd/spell/stop > /usr/dict/hstop || exit 1
echo 'apple banana' | spell > spell.out || exit 1
test ! -s spell.out || exit 1
echo 'zxqvnonword' | spell > misspell.out || exit 1
echo 'zxqvnonword' > misspell.want || exit 1
cmp misspell.out misspell.want || exit 1
man -Tlp cat > man.out || exit 1
grep NAME man.out || exit 1
mkfs fs.img 1000 || exit 1
normal /bin/icheck fs.img > icheck.out || exit 1
normal /bin/dcheck fs.img > dcheck.out || exit 1
normal /bin/ncheck fs.img > ncheck.out || exit 1
echo 'All userland package checks passed' || exit 1
sync || exit 1
