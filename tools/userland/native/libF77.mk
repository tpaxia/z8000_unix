CC=/bin/cc
CFLAGS=-O -Dunix=1 -Dz8000 -Dz8002
all: libF77.a
libF77.a: main.b s_rnge.b abort_.b getarg_.b iargc_.b signal_.b s_stop.b s_paus.b pow_ci.b pow_dd.b pow_di.b pow_hh.b pow_ii.b pow_ri.b pow_zi.b pow_zz.b c_abs.b c_cos.b c_div.b c_exp.b c_log.b c_sin.b c_sqrt.b z_abs.b z_cos.b z_div.b z_exp.b z_log.b z_sin.b z_sqrt.b r_abs.b r_acos.b r_asin.b r_atan.b r_atn2.b r_cnjg.b r_cos.b r_cosh.b r_dim.b r_exp.b r_imag.b r_int.b r_lg10.b r_log.b r_mod.b r_nint.b r_sign.b r_sin.b r_sinh.b r_sqrt.b r_tan.b r_tanh.b d_abs.b d_acos.b d_asin.b d_atan.b d_atn2.b d_cnjg.b d_cos.b d_cosh.b d_dim.b d_exp.b d_imag.b d_int.b d_lg10.b d_log.b d_mod.b d_nint.b d_prod.b d_sign.b d_sin.b d_sinh.b d_sqrt.b d_tan.b d_tanh.b i_abs.b i_dim.b i_dnnt.b i_indx.b i_len.b i_mod.b i_nint.b i_sign.b h_abs.b h_dim.b h_dnnt.b h_indx.b h_len.b h_mod.b h_nint.b h_sign.b l_ge.b l_gt.b l_le.b l_lt.b hl_ge.b hl_gt.b hl_le.b hl_lt.b s_cat.b s_cmp.b s_copy.b cabs.b
	/bin/rm -f libF77.a
	/bin/ar qc libF77.a main.b s_rnge.b abort_.b getarg_.b iargc_.b signal_.b s_stop.b s_paus.b pow_ci.b pow_dd.b pow_di.b pow_hh.b pow_ii.b pow_ri.b pow_zi.b
	/bin/ar qc libF77.a pow_zz.b c_abs.b c_cos.b c_div.b c_exp.b c_log.b c_sin.b c_sqrt.b z_abs.b z_cos.b z_div.b z_exp.b z_log.b z_sin.b z_sqrt.b
	/bin/ar qc libF77.a r_abs.b r_acos.b r_asin.b r_atan.b r_atn2.b r_cnjg.b r_cos.b r_cosh.b r_dim.b r_exp.b r_imag.b r_int.b r_lg10.b r_log.b r_mod.b
	/bin/ar qc libF77.a r_nint.b r_sign.b r_sin.b r_sinh.b r_sqrt.b r_tan.b r_tanh.b d_abs.b d_acos.b d_asin.b d_atan.b d_atn2.b d_cnjg.b d_cos.b d_cosh.b
	/bin/ar qc libF77.a d_dim.b d_exp.b d_imag.b d_int.b d_lg10.b d_log.b d_mod.b d_nint.b d_prod.b d_sign.b d_sin.b d_sinh.b d_sqrt.b d_tan.b d_tanh.b
	/bin/ar qc libF77.a i_abs.b i_dim.b i_dnnt.b i_indx.b i_len.b i_mod.b i_nint.b i_sign.b h_abs.b h_dim.b h_dnnt.b h_indx.b h_len.b h_mod.b h_nint.b
	/bin/ar qc libF77.a h_sign.b l_ge.b l_gt.b l_le.b l_lt.b hl_ge.b hl_gt.b hl_le.b hl_lt.b s_cat.b s_cmp.b s_copy.b cabs.b
main.b: /usr/src/libF77/main.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/main.c
s_rnge.b: /usr/src/libF77/s_rnge.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/s_rnge.c
abort_.b: /usr/src/libF77/abort_.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/abort_.c
getarg_.b: /usr/src/libF77/getarg_.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/getarg_.c
iargc_.b: /usr/src/libF77/iargc_.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/iargc_.c
signal_.b: /usr/src/libF77/signal_.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/signal_.c
s_stop.b: /usr/src/libF77/s_stop.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/s_stop.c
s_paus.b: /usr/src/libF77/s_paus.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/s_paus.c
pow_ci.b: /usr/src/libF77/pow_ci.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/pow_ci.c
pow_dd.b: /usr/src/libF77/pow_dd.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/pow_dd.c
pow_di.b: /usr/src/libF77/pow_di.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/pow_di.c
pow_hh.b: /usr/src/libF77/pow_hh.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/pow_hh.c
pow_ii.b: /usr/src/libF77/pow_ii.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/pow_ii.c
pow_ri.b: /usr/src/libF77/pow_ri.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/pow_ri.c
pow_zi.b: /usr/src/libF77/pow_zi.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/pow_zi.c
pow_zz.b: /usr/src/libF77/pow_zz.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/pow_zz.c
c_abs.b: /usr/src/libF77/c_abs.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/c_abs.c
c_cos.b: /usr/src/libF77/c_cos.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/c_cos.c
c_div.b: /usr/src/libF77/c_div.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/c_div.c
c_exp.b: /usr/src/libF77/c_exp.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/c_exp.c
c_log.b: /usr/src/libF77/c_log.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/c_log.c
c_sin.b: /usr/src/libF77/c_sin.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/c_sin.c
c_sqrt.b: /usr/src/libF77/c_sqrt.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/c_sqrt.c
z_abs.b: /usr/src/libF77/z_abs.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/z_abs.c
z_cos.b: /usr/src/libF77/z_cos.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/z_cos.c
z_div.b: /usr/src/libF77/z_div.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/z_div.c
z_exp.b: /usr/src/libF77/z_exp.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/z_exp.c
z_log.b: /usr/src/libF77/z_log.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/z_log.c
z_sin.b: /usr/src/libF77/z_sin.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/z_sin.c
z_sqrt.b: /usr/src/libF77/z_sqrt.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/z_sqrt.c
r_abs.b: /usr/src/libF77/r_abs.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_abs.c
r_acos.b: /usr/src/libF77/r_acos.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_acos.c
r_asin.b: /usr/src/libF77/r_asin.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_asin.c
r_atan.b: /usr/src/libF77/r_atan.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_atan.c
r_atn2.b: /usr/src/libF77/r_atn2.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_atn2.c
r_cnjg.b: /usr/src/libF77/r_cnjg.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_cnjg.c
r_cos.b: /usr/src/libF77/r_cos.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_cos.c
r_cosh.b: /usr/src/libF77/r_cosh.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_cosh.c
r_dim.b: /usr/src/libF77/r_dim.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_dim.c
r_exp.b: /usr/src/libF77/r_exp.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_exp.c
r_imag.b: /usr/src/libF77/r_imag.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_imag.c
r_int.b: /usr/src/libF77/r_int.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_int.c
r_lg10.b: /usr/src/libF77/r_lg10.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_lg10.c
r_log.b: /usr/src/libF77/r_log.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_log.c
r_mod.b: /usr/src/libF77/r_mod.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_mod.c
r_nint.b: /usr/src/libF77/r_nint.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_nint.c
r_sign.b: /usr/src/libF77/r_sign.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_sign.c
r_sin.b: /usr/src/libF77/r_sin.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_sin.c
r_sinh.b: /usr/src/libF77/r_sinh.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_sinh.c
r_sqrt.b: /usr/src/libF77/r_sqrt.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_sqrt.c
r_tan.b: /usr/src/libF77/r_tan.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_tan.c
r_tanh.b: /usr/src/libF77/r_tanh.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/r_tanh.c
d_abs.b: /usr/src/libF77/d_abs.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_abs.c
d_acos.b: /usr/src/libF77/d_acos.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_acos.c
d_asin.b: /usr/src/libF77/d_asin.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_asin.c
d_atan.b: /usr/src/libF77/d_atan.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_atan.c
d_atn2.b: /usr/src/libF77/d_atn2.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_atn2.c
d_cnjg.b: /usr/src/libF77/d_cnjg.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_cnjg.c
d_cos.b: /usr/src/libF77/d_cos.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_cos.c
d_cosh.b: /usr/src/libF77/d_cosh.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_cosh.c
d_dim.b: /usr/src/libF77/d_dim.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_dim.c
d_exp.b: /usr/src/libF77/d_exp.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_exp.c
d_imag.b: /usr/src/libF77/d_imag.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_imag.c
d_int.b: /usr/src/libF77/d_int.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_int.c
d_lg10.b: /usr/src/libF77/d_lg10.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_lg10.c
d_log.b: /usr/src/libF77/d_log.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_log.c
d_mod.b: /usr/src/libF77/d_mod.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_mod.c
d_nint.b: /usr/src/libF77/d_nint.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_nint.c
d_prod.b: /usr/src/libF77/d_prod.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_prod.c
d_sign.b: /usr/src/libF77/d_sign.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_sign.c
d_sin.b: /usr/src/libF77/d_sin.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_sin.c
d_sinh.b: /usr/src/libF77/d_sinh.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_sinh.c
d_sqrt.b: /usr/src/libF77/d_sqrt.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_sqrt.c
d_tan.b: /usr/src/libF77/d_tan.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_tan.c
d_tanh.b: /usr/src/libF77/d_tanh.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/d_tanh.c
i_abs.b: /usr/src/libF77/i_abs.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/i_abs.c
i_dim.b: /usr/src/libF77/i_dim.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/i_dim.c
i_dnnt.b: /usr/src/libF77/i_dnnt.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/i_dnnt.c
i_indx.b: /usr/src/libF77/i_indx.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/i_indx.c
i_len.b: /usr/src/libF77/i_len.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/i_len.c
i_mod.b: /usr/src/libF77/i_mod.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/i_mod.c
i_nint.b: /usr/src/libF77/i_nint.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/i_nint.c
i_sign.b: /usr/src/libF77/i_sign.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/i_sign.c
h_abs.b: /usr/src/libF77/h_abs.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/h_abs.c
h_dim.b: /usr/src/libF77/h_dim.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/h_dim.c
h_dnnt.b: /usr/src/libF77/h_dnnt.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/h_dnnt.c
h_indx.b: /usr/src/libF77/h_indx.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/h_indx.c
h_len.b: /usr/src/libF77/h_len.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/h_len.c
h_mod.b: /usr/src/libF77/h_mod.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/h_mod.c
h_nint.b: /usr/src/libF77/h_nint.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/h_nint.c
h_sign.b: /usr/src/libF77/h_sign.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/h_sign.c
l_ge.b: /usr/src/libF77/l_ge.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/l_ge.c
l_gt.b: /usr/src/libF77/l_gt.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/l_gt.c
l_le.b: /usr/src/libF77/l_le.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/l_le.c
l_lt.b: /usr/src/libF77/l_lt.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/l_lt.c
hl_ge.b: /usr/src/libF77/hl_ge.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/hl_ge.c
hl_gt.b: /usr/src/libF77/hl_gt.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/hl_gt.c
hl_le.b: /usr/src/libF77/hl_le.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/hl_le.c
hl_lt.b: /usr/src/libF77/hl_lt.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/hl_lt.c
s_cat.b: /usr/src/libF77/s_cat.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/s_cat.c
s_cmp.b: /usr/src/libF77/s_cmp.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/s_cmp.c
s_copy.b: /usr/src/libF77/s_copy.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/s_copy.c
cabs.b: /usr/src/libF77/cabs.c
	$(CC) $(CFLAGS) -c /usr/src/libF77/cabs.c
install: all
	/bin/cp libF77.a /lib/libF77.a
clean:
	/bin/rm -f *.b libF77.a
