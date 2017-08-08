struct test {
	int a, b, c;
};

int d, e, f;
int func(struct test *t) {
	d = t->a;
	e = t->b;
	f = t->c;
	t->a = t->a + 1;
	t->b = t->b + 1;
	t->c = t->c + 1;
}

struct test t;
t.a = 5;
t.b = 6;
t.c = 7;
func(&t);
return d + e + f + t.a + t.b + t.c - 39;
