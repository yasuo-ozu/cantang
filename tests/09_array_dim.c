int a[3][5], i, j;
for (i = 0; i < 3; i++) {
	for (j = 0; j < 5; j++) {
		a[i][j] = i * 10 + j;
	}
}
return a[2][4] - 24 + a[1][2] - 12;
