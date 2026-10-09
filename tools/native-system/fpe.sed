1i\
.unsegm\
.global epu
s/;.*//
s/^[ 	]*//
s/[ 	]*$//
s/,$//
/^$/d
/^\.input/d
/^\.eject/d
s/__text[ 	][ 	]*\.sect/.text/
s/__data[ 	][ 	]*\.sect/.data/
s/\([0-9][0-9a-fA-F]*\)h/0x\1/g
s/r\([0-9][0-9]*\)(#\([^)]*\))/\2(r\1)/g
s/\.block/.space/g
s/^Fsetmode$/Fsetmode:/
s/^\(rrc*b*\)[ 	][ 	]*\(r[hl]*[0-9][0-9]*\)$/\1	\2,#1/
s/^\(rlc*b*\)[ 	][ 	]*\(r[hl]*[0-9][0-9]*\)$/\1	\2,#1/
s/^\(incb*\)[ 	][ 	]*\(r[hl]*[0-9][0-9]*\)$/\1	\2,#1/
s/^\(decb*\)[ 	][ 	]*\(r[hl]*[0-9][0-9]*\)$/\1	\2,#1/
