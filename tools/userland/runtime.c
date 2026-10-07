#include <a.out.h>
#include <mp.h>

struct nlist names[] = { "_answer", 0, 0, "_absent", 0, 0, "", 0, 0 };

main()
{
	int fds[2];
	char c;
	MINT *a, *b, q, r;
	short rem;

	if (nlist("obj.b", names) != 0) return 1;
	if (names[0].n_type != (N_TEXT|N_EXT) || names[1].n_type) return 2;
	if (pipe(fds) < 0 || dup2(fds[1], 9) < 0) return 3;
	close(fds[1]);
	if (write(9, "x", 1) != 1 || read(fds[0], &c, 1) != 1 || c != 'x') return 4;
	close(9); close(fds[0]);
	a = itom(32767); b = itom(32767); q.len = r.len = 0;
	mult(a, b, &q);
	mdiv(&q, a, &q, &r);
	if (mcmp(&q, b) || r.len) return 5;
	sdiv(&q, 7, &q, &rem);
	if (q.len != 1 || q.val[0] != 4681 || rem != 0) return 6;
	return 0;
}
