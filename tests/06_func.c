int func(int a, int b) {
	return a * b;
}

int factor(int a) {
	if (a == 0) { return 1; }
	return a * factor(a - 1);
}
return func(3, 5) + factor(5) - 120 - 15;
