.PHONY: clean test
cantang:	main.c
	gcc $< -o $@ -g3 -Wall -Wextra
	wc -l $<

clean:
	rm -rf cantang

test:	cantang
	./$< tests/00_return.c
	./$< tests/01_while.c
	./$< tests/02_assign.c
	./$< tests/03_op.c
	@echo "test pass"
