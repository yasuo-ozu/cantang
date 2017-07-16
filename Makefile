.PHONY: clean
cantang:	main.c
	gcc $< -o $@ -g3 -Wall -Wextra

clean:
	rm -rf cantang
