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
int strlen(char *s) {
	int i;
	for (i = 0; s[i] != 0; i++);
	return i;
}

char *strdup(char *s) {
	int size = strlen(s) + 1;
	char *t = malloc(size);
	memcpy(t, s, size);
	return t;
}

int strcmp(char *s1, char *s2) {
	int i;
	for (i = 0; 1; i++) {
		if (s1[i] != s2[i] || s1[i] == 0) return s2[i] - s1[i];
	}
	return 0;
}

char *strchr(char *s, int c) {
	int i;
	for (i = 0; s[i] != 0; i++)
		if (s[i] == c) return s + i;
	return 0;
}

puts strdup("hello, world\n");
puts strdup("Hi!\n");
print strcmp("abcd", "abcd");
print strcmp("abcd", "abce");
print strcmp("abcd", "ab");


