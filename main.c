#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef enum {
	T_NULL = 0,		// トークンが使用されていない、またはトークン列の終端を表す
	T_KEYWORD,		// キーワード(int, forなど)
	T_INTVAL,		// 数値リテラル
	T_IDENT,		// 変数名など
	T_SYMBOL		// 演算子
} tokenType;

typedef struct token {
	tokenType type;
	char *text;		// 対応するソースコード。T_IDENT, T_SYMBOLまたはT_KEYWORDのときに使う
	int intval;		// 整数。T_INTVALのときに使う
	int symbol;		// symbols[]内のインデックス。T_SYMBOLの時に使う
} token;

typedef struct {
	token *token;
} context;

/* グローバル変数 */
const char * const keywords[] = {
	"int",
	"print",
	"return",
	NULL
};

struct {
	const char * const text;
	int priority[3];	// PRE, POST, BINARY
} symbols[] = {
	{NULL,  { 0,  0,  0}},
	{"++",  { 2,  1,  0}}, {"--",  { 2,  1,  0}}, {"~",   { 2,  0,  0}}, {"!",   { 2,  0,  0}},
	{"*",   { 0,  0,  4}}, {"/",   { 0,  0,  4}}, {"%",   { 0,  0,  4}},
	{"+",   { 2,  0,  5}}, {"-",   { 2,  0,  5}},
	{"<<",  { 0,  0,  6}}, {">>",  { 0,  0,  6}},
	{"<",   { 0,  0,  7}}, {"<=",  { 0,  0,  7}}, {">",   { 0,  0,  7}}, {">=",  { 0,  0,  7}},
	{"==",  { 0,  0,  8}}, {"!=",  { 0,  0,  8}},
	{"&",   { 0,  0,  9}}, {"^",   { 0,  0, 10}}, {"|",   { 0,  0, 11}},
	{"&&",  { 0,  0, 12}}, {"||",  { 0,  0, 13}},
	{"=",   { 0,  0, 15}}, {"+=",  { 0,  0, 15}}, {"-=",  { 0,  0, 15}}, {"*=",  { 0,  0, 15}},
	{"/=",  { 0,  0, 15}}, {"%=",  { 0,  0, 15}}, {"<<=", { 0,  0, 15}}, {">>=", { 0,  0, 15}},
	{"&=",  { 0,  0, 15}}, {"^=",  { 0,  0, 15}}, {"|=",  { 0,  0, 15}},
	{",",   { 0,  0, 16}}, {"(",   { 0,  0,  0}}, {")",   { 0,  0,  0}}, {"{",   { 0,  0,  0}},
	{"}",   { 0,  0,  0}}, {"[",   { 0,  0,  0}}, {"]",   { 0,  0,  0}}, {"?",   { 0,  0,  0}},
	{":",   { 0,  0,  0}}, {";",   { 0,  0,  0}},
	{NULL,  { 0,  0,  0}}
};

int cmp(context *ctx, const char *s) {
	return ctx->token->type != T_NULL && strcmp(ctx->token->text, s) == 0;
}

int cmp_skip(context *ctx, const char *s) {
	int ret = cmp(ctx, s);
	if (ret) ctx->token++;
	return ret;
}

void cmp_err(context *ctx, const char *s) {
	int ret = cmp_skip(ctx, s);
	if (!ret) {
		fprintf(stderr, "require %s\n", s);
		exit(1);
	}
}

void cmp_err_skip(context *ctx, const char *s) {
	int ret = cmp_skip(ctx, s);
	if (!ret) {
		fprintf(stderr, "require %s\n", s);
		exit(1);
	}
}

typedef struct map {
	struct map *next;
	char *key;
	void *value;
} map;

void map_free(map *m) {
	while (m != NULL) {
		map *n = m->next;
		free(m->value);
		free(m);
		m = n;
	}
}

map *map_add(map *m, char *key, void *value) {
	map *n = malloc(sizeof(map));
	n->next = m;
	n->key = key;
	n->value = value;
	return n;
}

void *map_search(map *m, const char *key) {
	while (m != NULL) {
		if (strcmp(m->key, key) == 0) return m->value;
		m = m->next;
	}
	return NULL;
}

typedef struct {
	enum {
		VT_INT, VT_FUNC
	} type;
	int intval;
} variable;

typedef struct block {
	struct block *parent;
	map *table;
} block;

variable *search(block *blk, const char *key) {
	while (blk != NULL) {
		variable *b = (variable *) map_search(blk->table, key);
		if (b != NULL) return b;
		blk = blk->parent;
	}
	return NULL;
}

int proceed_binary_operator(token *op, int a, int b) {
	if      (strcmp(op->text, "*" ) == 0) return a *  b;
	else if (strcmp(op->text, "/" ) == 0) return a /  b;
	else if (strcmp(op->text, "%" ) == 0) return a %  b;
	else if (strcmp(op->text, "+" ) == 0) return a +  b;
	else if (strcmp(op->text, "-" ) == 0) return a -  b;
	else if (strcmp(op->text, "<<") == 0) return a << b;
	else if (strcmp(op->text, ">>") == 0) return a >> b;
	else if (strcmp(op->text, "<" ) == 0) return a <  b;
	else if (strcmp(op->text, "<=") == 0) return a <= b;
	else if (strcmp(op->text, ">" ) == 0) return a >  b;
	else if (strcmp(op->text, ">=") == 0) return a >= b;
	else if (strcmp(op->text, "==") == 0) return a == b;
	else if (strcmp(op->text, "!=") == 0) return a != b;
	else if (strcmp(op->text, "&" ) == 0) return a &  b;
	else if (strcmp(op->text, "^" ) == 0) return a ^  b;
	else if (strcmp(op->text, "|" ) == 0) return a |  b;
	else if (strcmp(op->text, "&&") == 0) return a && b;
	else if (strcmp(op->text, "||") == 0) return a || b;
	else if (strcmp(op->text, "," ) == 0) return b;
	else {
		fprintf(stderr, "Not implemented: %s\n", op->text);
		exit(1);
	}
}

int proceed_expression(context *, block *, int, int);
int proceed_expression_internal(context *ctx, block *blk, int isVector, int priority, int ef) {
	int ret = 0;
	if (priority == 1) {
		if (ctx->token->type == T_INTVAL) {
			if (ef) ret = ctx->token->intval;
			ctx->token++;
		} else if (cmp_skip(ctx, "(")) {
			ret = proceed_expression(ctx, blk, 0, ef);
			cmp_err_skip(ctx, ")");
		} else {
			if (ef) {
				variable *var = search(blk, ctx->token->text);
				if (var != NULL) {
					ret = var->intval;
				} else {
					fprintf(stderr, "Invalid terminal term: %s\n", ctx->token->text);
					exit(1);
				}
			}
			ctx->token++;
		}
	} else if (symbols[ctx->token->symbol].priority[0] == priority) {
		ret = proceed_expression_internal(ctx, blk, isVector, priority, ef);
		if      (cmp_skip(ctx, "+")) ret = +ret;
		else if (cmp_skip(ctx, "-")) ret = -ret;
		else if (cmp_skip(ctx, "~")) ret = ~ret;
		else if (cmp_skip(ctx, "!")) ret = !ret;
		else {
			fprintf(stderr, "Not implemented: %s\n", ctx->token->text);
			exit(1);
		}
	} else {
		token *op;
		ret = proceed_expression_internal(ctx, blk, isVector, priority - 1, ef);
		if (symbols[ctx->token->symbol].priority[2] == priority
				&& !(isVector && cmp(ctx, ","))) {
			if (priority == 14 || priority == 15) {
				op = ctx->token++;
				int right = proceed_expression_internal(ctx, blk, isVector, priority, ef);
				if (ef) ret = proceed_binary_operator(op, ret, right);
			} else {
				do {
					int effective = 1;
					if (cmp(ctx, "&&")) effective = ret;
					else if (cmp(ctx, "||")) effective = !ret;
					op = ctx->token++;
					int right = proceed_expression_internal(ctx, blk, isVector, priority - 1, ef && effective);
					if (ef) ret = proceed_binary_operator(op, ret, right);
				} while (symbols[ctx->token->symbol].priority[2] == priority);
			}
		} else if (priority == 14 && cmp_skip(ctx, "?")) {
			int a = proceed_expression_internal(ctx, blk, isVector, priority, ef);
			cmp_err_skip(ctx, ":");
			int b = proceed_expression_internal(ctx, blk, isVector, priority, ef);
			ret = ret ? a : b;
		} else {
			while (symbols[ctx->token->symbol].priority[1] == priority) {
				fprintf(stderr, "Not implimented: %s\n", ctx->token->text);
				exit(1);
			}
		}
	}
	return ret;

}

int proceed_expression(context *ctx, block *blk, int isVector, int ef) {
	return proceed_expression_internal(ctx, blk, isVector, 16, ef);
}

int proceed_block(context *ctx, block *parent, int ef) {
	int ret, result = 0;
	block blk;
	blk.parent = parent;
	blk.table = NULL;
	while (ctx->token->type != T_NULL) {
		if (cmp_skip(ctx, "print")) {
			ret = proceed_expression(ctx, &blk, 0, ef);
			if (ef) printf("%d\n", ret);
			cmp_err_skip(ctx, ";");
		} else if (cmp_skip(ctx, "return")) {
			ret = proceed_expression(ctx, &blk, 0, ef);
			if (ef) result = ret, ef = 0;
			cmp_err_skip(ctx, ";");
		} else if (cmp_skip(ctx, "int")) {
			do {
				if (ef && (ctx->token->type != T_IDENT ||
					search(&blk, ctx->token->text) != NULL)) {
					fprintf(stderr, "Bad variable name: %s\n", ctx->token->text);
					exit(1);
				}
				char *name = ctx->token->text;
				ctx->token++;
				if (ret = 0, cmp_skip(ctx, "=")) ret = proceed_expression(ctx, &blk, 1, ef);
				if (ef) {
					variable *var = malloc(sizeof(variable));
					var->intval = ret;
					var->type = VT_INT;
					blk.table = map_add(blk.table, name, var);
				}
			} while (cmp_skip(ctx, ","));
			cmp_err_skip(ctx, ";");
		} else if (cmp_skip(ctx, "{")) {
			proceed_block(ctx, &blk, ef);
			cmp_err_skip(ctx, "}");
		} else {
			proceed_expression(ctx, &blk, 0, ef);
			cmp_err_skip(ctx, ";");
		}
	}
	map_free(blk.table);
	return result;
}

/* token構造体の配列を実行します */
int proceed(context *ctx) {
	return proceed_block(ctx, NULL, 1);
}

void dump_token_vector(token *tkn) {
	while (1) {
		if (tkn->type == T_KEYWORD) {
			printf("KEYWORD: %s\n", tkn->text);
		} else if (tkn->type == T_IDENT) {
			printf("IDENT: %s\n", tkn->text);
		} else if (tkn->type == T_SYMBOL) {
			printf("SYMBOL: %s\n", tkn->text);
		} else if (tkn->type == T_INTVAL) {
			printf("INT: %d\n", tkn->intval);
		} else break;
		tkn++;
	}
}

/* ソースファイルのfpを受け取り、token構造体の配列を返します */
token *create_token_vector(FILE *fp) {
	token *tok = malloc(sizeof(tok) * 1024);
	int i = 0;
	int c = fgetc(fp);
	while (1) {
		while (1) {
			while (strchr("\r\n\t\v ", c) != NULL) c = fgetc(fp);
			if (c == '/') {
				c = fgetc(fp);
				if (c == '*') {
					while(c != EOF) {
						c = fgetc(fp);
						if (c == '*') {
							c = fgetc(fp);
							if (c == '/') {
								c = fgetc(fp);
								break;
							}
						}
					}
					continue;
				} else if (c == '/') {
					while (c != '\n' && c != EOF) c = fgetc(fp);
					continue;
				} else {
					ungetc(c, fp);
					c = '/';
				}
			}
			break;
		}
		tok[i].symbol = 0;
		if (c == EOF) break;
		if ('0' <= c && c <= '9') {
			tok[i].type = T_INTVAL;
			int val = 0;
			do {
				int digit = c - '0';
				val = val * 10 + digit;
				c = fgetc(fp);
			} while ('0' <= c && c <= '9');
			tok[i].intval = val;
		} else {
			char s[256];
			int j = 0;
			if (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || c == '_') {
				do {
					s[j] = (char) c;
					j++;
					c = fgetc(fp);
				} while (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') 
						|| c == '_' || ('0' <= c && c <= '9'));
				s[j] = '\0';
				for (j = 0; keywords[j] != NULL; j++) {
					if (strcmp(keywords[j], s) == 0) break;
				}
				tok[i].type = keywords[j] == NULL ? T_IDENT : T_KEYWORD;
			} else {
				tok[i].type = T_SYMBOL;
				int k, symb;
				while (c != EOF) {
					s[j] = (char) c;
					s[j + 1] = '\0';
					for (k = 1; symbols[k].text != NULL; k++) {
						if (strcmp(symbols[k].text, s) == 0) break;
					}
					if (symbols[k].text == NULL) break;
					symb = k;
					c = fgetc(fp);
					j++;
				}
				s[j] = '\0';
				if (j == 0) {
					fprintf(stderr, "Bad char: %c\n", s[0]);
					exit(1);
				}
				tok[i].symbol = symb;
			}
			tok[i].text = strdup(s);
		}
		i++;
	}
	tok[i].type = T_NULL;
	return tok;
}

int main(int argc, char **argv) {
	if (argc == 2) {
		char *fname = argv[1];
		FILE *fp;
		if (strcmp(fname, "-") == 0) fp = stdin;
		else fp = fopen(fname, "rb");
		if (fp == NULL) {
			fprintf(stderr, "File open error: %s\n", fname);
			return 1;
		}
		context ctx;
		ctx.token = create_token_vector(fp);
		if (fp != stdin) fclose(fp);
		// dump_token_vector(ctx.token);
		return proceed(&ctx);
	}
	printf("cantang -- a tiny interpreter\n"
			"\n"
			"Usage:\n"
			"    %s filename\n"
			"\n",
			argv[0]
		);
}

// vim: ts=4 ai sw=4 :
