#include <dbm.h>

main()
{
	datum key, value, got;
	int fd;
	fd = creat("db.dir", 0600); if (fd < 0) return 1; close(fd);
	fd = creat("db.pag", 0600); if (fd < 0) return 2; close(fd);
	if (dbminit("db") < 0) return 3;
	key.dptr = "key"; key.dsize = 3;
	value.dptr = "value"; value.dsize = 5;
	if (store(key, value) < 0) return 4;
	got = fetch(key);
	if (got.dsize != 5 || strncmp(got.dptr, "value", 5)) return 5;
	if (delete(key) < 0 || fetch(key).dptr != 0) return 6;
	return 0;
}
