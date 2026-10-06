/* Portable archive framing; object contents retain their target format.
 * Short member names fit the V7 filesystem's 14-character limit.
 */
#define ARMAG "!<arch>\n"
#define SARMAG 8
#define ARFMAG "`\n"
struct ar_disk {
    char name[16], date[12], uid[6], gid[6], mode[8], size[10], end[2];
};
struct ar_member {
    char ar_name[15];
    long ar_date, ar_size;
    unsigned ar_uid, ar_gid, ar_mode;
};

static long
arnum(p, n, base)
char *p;
int n, base;
{
    long value;
    int digit, seen, trailing;
    value = 0; seen = trailing = 0;
    while (n--) {
        digit = *p++;
        if (digit == ' ') { if (seen) trailing = 1; continue; }
        digit -= '0';
        if (trailing || digit < 0 || digit >= base ||
            value > (2147483647L-digit)/base) return -1L;
        value = value*base + digit; seen = 1;
    }
    return seen ? value : 0L;
}

static
ardecode(d, m)
struct ar_disk *d;
struct ar_member *m;
{
    int n, i;
    long uid, gid, mode;
    if (d->end[0] != '`' || d->end[1] != '\n') return 0;
    n = 16;
    while (n && d->name[n-1] == ' ') --n;
    if (n && d->name[n-1] == '/') --n;
    if (!n || n > 14) return 0;
    for (i=0; i<n; i++) {
        if (d->name[i] == '/' || !d->name[i]) return 0;
        m->ar_name[i] = d->name[i];
    }
    while (i<15) m->ar_name[i++] = 0;
    m->ar_date = arnum(d->date,12,10);
    m->ar_size = arnum(d->size,10,10);
    uid = arnum(d->uid,6,10); gid = arnum(d->gid,6,10);
    mode = arnum(d->mode,8,8);
    if (m->ar_date < 0 || m->ar_size < 0 || uid < 0 || uid > 65535L ||
        gid < 0 || gid > 65535L || mode < 0 || mode > 65535L) return 0;
    m->ar_uid = uid; m->ar_gid = gid; m->ar_mode = mode;
    return 1;
}
