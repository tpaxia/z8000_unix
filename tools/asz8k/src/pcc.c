/* Accept PCC/az8 source directly. No external assembly rewriting pass.
 * Instruction encoding remains in the imported assembler's format tables.
 */
#include <stdio.h>
#include "acom.h"
#include "asz8k.h"
#include "obj.h"

int pccflg, pccpass;
int machineflg, objectseg;
static unsigned branch;
static char longjr[1024], pending[1024]; /* 8192 branches, one bit each */
static int changed;

/* Condition aliases beginning with f are ordinary PCC runtime identifiers. */
pccsymbol(sym)
vmadr sym;
{
    struct sytab *s;
    s = (struct sytab *)rfetch(sym);
    if (s->sy_typ==STKEY && s->sy_str[0]=='f' &&
        (s->sy_val>>8)==TKCCODE) {
        s = (struct sytab *)wfetch(sym);
        s->sy_typ = STUND; s->sy_val = 0; s->sy_rel = RBUND;
        s->sy_atr &= ~SADP2;
    }
}

pccinit()
{
    struct sytab *s;
    s = (struct sytab *)wfetch(sylook("sp"));
    s->sy_typ = STKEY; s->sy_val = 0x110f; s->sy_atr = SADP2;
}

pccreset()
{
    vmadr p, next;
    struct sytab *s;
    unsigned h, i;
    branch = 0;
    for (i = 0; i < SECSIZ; i++) sectab[i].se_loc = 0;
    for (h = 0; h < (1<<SHSHLOG); h++) for (p = syhtab[h]; p; p = next) {
        s = (struct sytab *)wfetch(p); next = s->sy_lnk;
        if (s->sy_typ != STKEY) s->sy_atr &= ~SADP2;
    }
}

pccsect(name)
char *name;
{
    struct sytab *s;
    unsigned sec;
    label = sylook(name); s = (struct sytab *)wfetch(label);
    if (s->sy_typ != STSEC) newsec();
    else {
        sec = s->sy_rel; s->sy_atr |= SADP2;
        if (sec >= secct) secct = sec+1;
        setsec(sec);
    }
}

/* These decisions use a completed, unchanged layout. Apply promotions only
 * after that pass, then regenerate symbol positions before deciding again.
 * This prevents stale forward-label offsets from causing false promotions.
 */
pccrelax()
{
    unsigned i, rounds;
    for (rounds = 0; rounds < 8193; rounds++) {
        pccpass = 1; pccreset(); dopass();
        pccpass = 2; changed = 0; pccreset(); dopass();
        if (!changed) { pccpass = 0; return; }
        for (i = 0; i < sizeof(longjr); i++) { longjr[i] |= pending[i]; pending[i] = 0; }
    }
    fprintf(ERROR,"PCC branch relaxation did not converge\n"); exit(1);
}

struct format *
pccbranch(fmp)
struct format *fmp;
{
    unsigned slot, mask;
    int k, call;
    long span;
    call = !strcmp(opcstr,"calr");
    if (!call && strcmp(opcstr,"jr")) return fmp;
    if (branch >= 8192) { fprintf(ERROR,"PCC branch table overflow\n"); exit(1); }
    slot = branch>>3; mask = 1<<(branch++&7);
    k = call || optab[1].op_cls & (1L<<OCNULL) ? 0 : 1;
    span = (long)optab[k].op_val-curloc-2;
    if (pccpass == 2 && !(longjr[slot]&mask) &&
        (optab[k].op_rel != cursec || (span&1) ||
         span < (call ? -4094 : -256) || span > (call ? 4096 : 254))) {
        pending[slot] |= mask; changed = 1;
    }
    if (longjr[slot]&mask) return (struct format *)oclook(call ? "call" : "jp")->oc_val;
    return fmp;
}

static point()
{
    struct sytab *s;
    s = (struct sytab *)wfetch(sylook("."));
    s->sy_typ = STVAR; s->sy_val = curloc; s->sy_rel = cursec; s->sy_atr |= SADP2;
}

static value()
{
    iilex(); if (toktyp == TKSPC) iilex(); expression();
}

pccstmt()
{
    struct sytab *s;
    long n;
    vmadr sym;
    char name[SYMSIZ+1];
    point();
    if (machineflg) {
        if (!strcmp(opcstr,".segm")) { segflg = 1; return 1; }
        if (!strcmp(opcstr,".unsegm")) { segflg = 0; return 1; }
        if (!strcmp(opcstr,".global")) { direc(ADGLOB); return 1; }
        if (!strcmp(opcstr,".org") || !strcmp(opcstr,".space")) {
            value(); n = curop.op_val;
            if (!strcmp(opcstr,".space")) n += curloc;
            if (curop.op_rel || n < curloc || n > 65535L) err('E');
            else curloc = n;
            return 1;
        }
    }
    if (toktyp == TKRELOP && tokval == TVEQ &&
        (!strcmp(opcstr,".") || (!*opcstr && *labstr))) {
        if (*opcstr) {
            label = *labstr ? sylook(labstr) : 0;
            assign(STLAB,curloc,cursec);
        }
        value();
        if (*opcstr || !strcmp(labstr,".")) {
            if (curop.op_rel != cursec || curop.op_val < curloc) err('E');
            else curloc = curop.op_val;
        } else { label = sylook(labstr); assign(STLAB,curop.op_val,curop.op_rel); }
        return 1;
    }
    if (!strcmp(opcstr,".text")) { pccsect("__text"); return 1; }
    if (!strcmp(opcstr,".data")) { pccsect("__data"); return 1; }
    if (!strcmp(opcstr,".bss")) { pccsect("__bss"); return 1; }
    if (!strcmp(opcstr,".globl")) { direc(ADGLOB); return 1; }
    if (!strcmp(opcstr,".even")) {
        curloc = (curloc+1)&~1L;
        label = *labstr ? sylook(labstr) : 0; assign(STLAB,curloc,cursec);
        return 1;
    }
    if (!strcmp(opcstr,".comm")) {
        if (toktyp != TKSPC || token() != TKSYM) { err('S'); skipeol(); return 1; }
        symcpy(name,tokstr); name[SYMSIZ] = 0; sym = sylook(name);
        if (token() != TKCOM) { err('S'); skipeol(); return 1; }
        value();
        if (curop.op_rel || curop.op_val < 0 || curop.op_val > 65535L) err('E');
        else if (!pass2) {
            s = (struct sytab *)wfetch(sym);
            if (s->sy_typ == STUND || s->sy_typ == STCOM) {
                s->sy_typ = STCOM; s->sy_rel = RBUND;
                if (s->sy_val < curop.op_val) s->sy_val = curop.op_val;
                s->sy_atr |= SACOM|SAGLO;
            }
        }
        return 1;
    }
    if (!strcmp(opcstr,".zerow")) {
        label = *labstr ? sylook(labstr) : 0; assign(STLAB,curloc,cursec);
        value(); n = (long)curop.op_val*2;
        if (curop.op_rel || n < 0 || n > 65535L) err('E');
        else {
            s = (struct sytab *)rfetch(sectab[cursec].se_sym);
            if (!strcmp(s->sy_str,"__bss")) curloc += n;
            else while (n-- > 0) emitb(0,0);
        }
        return 1;
    }
    return 0;
}
