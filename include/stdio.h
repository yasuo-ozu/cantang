int stdin = 0, stdout = 1, stderr = 2;

int printf(char *format, int val) {
	return fprintf(stdout, format, val);
}

int putint(int fd, int val) {
	char *s = "s";
	int i = 1;
	while (val / i > 0) i = i * 10;
	if (i > 1) i = i / 10;
	while (i) {
		s[0] = (val / i) + 48;	// 48 == '0'
		puts s;
		val = val % i;
		i = i / 10;
	}
}

int putc(int c, int fd) {
	char *s = "S";
	s[0] = c;
	puts s;
	// TODO: *s = c とすると sが変化してしまう
}

int putchar(int c) {
	putc(c, stdout);
}

int fprintf(int fd, char *format, int val) {
	char c;
	int i;
	for (i = 0; 1; i++) {
		c = format[i];
		if (c == 0) break;
		if (c == 37) {	// '%'
			i++;
			c = format[i];
			if (c == 115) {	// 's'
				puts val;
			} else {
				if (c == 108) { // 'l'
					i++;
					c = format[i];
					if (c == 108) i++;
				}
				putint(fd, val);
			}
			continue;
		}
		putc(c, fd);
	}
}

