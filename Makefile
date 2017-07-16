.PHONY: clean test
cantang:	main.c
	gcc $< -o $@ -g3 -Wall -Wextra
	wc -l $<

clean:
	rm -rf cantang

test:	cantang
	./$< tests/00_return.c
	@echo "test pass"
