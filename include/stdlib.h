int malloc(int size) {
	int a[size];
	return a;
}

int calloc(int count, int size) {
	size = count * size;
	int *a = malloc(size);
	int i;
	for (i = 0; i < size; i++) a[i] = 0;
	return a;
}

int memcpy(char *dest, char *src, int size) {
	int i;
	for (i = 0; i < size; i++) {
		dest[i] = src[i];
	}
}
