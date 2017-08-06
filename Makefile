.PHONY: clean test
cantang:	main.c
	gcc $< -o $@ -g3 -Wall -Wextra
	cat $< | sed -ne '/^./p' | wc -l

clean:
	rm -rf cantang

test:	cantang
	./$< tests/00_return.c
	./$< tests/01_while.c
	./$< tests/02_assign.c
	./$< tests/03_op.c
	./$< tests/04_array.c
	./$< tests/05_for.c
	./$< tests/06_func.c
	./$< tests/07_dowhile.c
	@echo "test pass"
