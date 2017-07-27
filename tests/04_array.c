int a[10], i = 0, j = 0;
while (i < 10) {
	a[i] = i * 10;
	i = i + 1;
}
i = 0;
while (i < 10) {
	j = j + a[i];
	i = i + 1;
}
return 450 - j;
