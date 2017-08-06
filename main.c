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
	long long intval;		// 整数。T_INTVALのときに使う
	int symbol;		// symbols[]内のインデックス。T_SYMBOLの時に使う
} token;

/* グローバル変数 */
const char * const keywords[] = {
	"int", "print", "puts", "return", "if", "break", "continue", "for", "while",
	NULL
};

struct {
	const char * const text;
	int priority[3];	// PRE, POST, BINARY
} symbols[] = {
	{NULL,  { 0,  0,  0}},
	{"++",  { 2,  1,  0}}, {"--",  { 2,  1,  0}}, {"~",   { 2,  0,  0}}, {"!",   { 2,  0,  0}},
	{"*",   { 2,  0,  4}}, {"/",   { 0,  0,  4}}, {"%",   { 0,  0,  4}},
	{"+",   { 2,  0,  5}}, {"-",   { 2,  0,  5}},
	{"<<",  { 0,  0,  6}}, {">>",  { 0,  0,  6}},
	{"<",   { 0,  0,  7}}, {"<=",  { 0,  0,  7}}, {">",   { 0,  0,  7}}, {">=",  { 0,  0,  7}},
	{"==",  { 0,  0,  8}}, {"!=",  { 0,  0,  8}},
	{"&",   { 0,  0,  9}}, {"^",   { 0,  0, 10}}, {"|",   { 0,  0, 11}},
	{"&&",  { 0,  0, 12}}, {"||",  { 0,  0, 13}},
	{"=",   { 0,  0, 15}}, {"+=",  { 0,  0, 15}}, {"-=",  { 0,  0, 15}}, {"*=",  { 0,  0, 15}},
	{"/=",  { 0,  0, 15}}, {"%=",  { 0,  0, 15}}, {"<<=", { 0,  0, 15}}, {">>=", { 0,  0, 15}},
	{"&=",  { 0,  0, 15}}, {"^=",  { 0,  0, 15}}, {"|=",  { 0,  0, 15}},
	{",",   { 0,  0, 16}}, {"(",   { 0,  1,  0}}, {")",   { 0,  0,  0}}, {"{",   { 0,  0,  0}},
	{"}",   { 0,  0,  0}}, {"[",   { 0,  1,  0}}, {"]",   { 0,  0,  0}}, {"?",   { 0,  0,  0}},
	{":",   { 0,  0,  0}}, {";",   { 0,  0,  0}},
	{NULL,  { 0,  0,  0}}
};

typedef struct map {
	struct map *next;
	char *key;
	void *value;
} map;

typedef struct {
	enum {
		VT_NULL = 0, VT_INT, VT_FUNC, VT_ARRAY
	} type;
	long long intval;
} variable;

typedef struct block {
	struct block *parent;
	map *table;
} block;

typedef struct {
	token *token;
	long long return_value;
	block *global;
} context;

#define err(...)	{ fprintf(stderr, __VA_ARGS__); exit(1); }

int cmp(context *ctx, const char *s) {
	return ctx->token->type != T_NULL && ctx->token->type != T_INTVAL && strcmp(ctx->token->text, s) == 0;
}

int cmp_skip(context *ctx, const char *s) {
	int ret = cmp(ctx, s);
	if (ret) ctx->token++;
	return ret;
}

void cmp_err_skip(context *ctx, const char *s) {
	int ret = cmp_skip(ctx, s);
	if (!ret) err("require %s", s);
}

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
	else err("Not implemented: %s\n", op->text);
}

int proceed_statement(context *, block *, int);
variable *proceed_expression(context *, block *, int, int);
variable *proceed_expression_internal(context *ctx, block *blk, int isVector, int priority, int ef) {
	variable *retvar = NULL;
	long long ret = 0, i;
	token *op;
	if (priority == 0) {
		if (ctx->token->type == T_INTVAL) {
			if (ef) ret = ctx->token->intval;
			ctx->token++;
			retvar = NULL;
		} else if (cmp_skip(ctx, "(")) {
			retvar = proceed_expression(ctx, blk, 0, ef);
			cmp_err_skip(ctx, ")");
		} else {
			if (ef) {
				retvar = search(blk, ctx->token->text);
				if (retvar == NULL) err("Invalid terminal term: %s\n", ctx->token->text);
			}
			ctx->token++;
		}
	} else if (symbols[ctx->token->symbol].priority[0] == priority) {
		op = ctx->token++;
		retvar = proceed_expression_internal(ctx, blk, isVector, priority, ef);
		ret = retvar->intval;
		if      (strcmp(op->text, "+") == 0) ret = +ret;
		else if (strcmp(op->text, "-") == 0) ret = -ret;
		else if (strcmp(op->text, "!") == 0) ret = !ret;
		else if (strcmp(op->text, "~") == 0) ret = ~ret;
		else if ((i = (strcmp(op->text, "++") == 0)) || strcmp(op->text, "--") == 0) {
			if (ef) ret = i ? ++retvar->intval : --retvar->intval;
		} else err("Not implemented: %s\n", ctx->token->text);
		retvar = NULL;	// You have to duplicate variable
	} else {
		retvar = proceed_expression_internal(ctx, blk, isVector, priority - 1, ef);
		if (ef) ret = retvar->intval;
		if (symbols[ctx->token->symbol].priority[2] == priority && !(isVector && cmp(ctx, ","))) {
			if (priority == 14 || priority == 15) {
				op = ctx->token++;
				variable *right = proceed_expression_internal(ctx, blk, isVector, priority, ef);
				if (ef) {
					if (strcmp(op->text, "=") == 0) ret = retvar->intval = right->intval;
					else ret = proceed_binary_operator(op, ret, right->intval);
				}
			} else {
				do {
					int effective = 1;
					if (cmp(ctx, "&&")) effective = ret;
					else if (cmp(ctx, "||")) effective = !ret;
					op = ctx->token++;
					variable *right = proceed_expression_internal(ctx, blk, isVector, priority - 1, ef && effective);
					if (ef) ret = proceed_binary_operator(op, ret, right->intval);
				} while (symbols[ctx->token->symbol].priority[2] == priority);
			}
			retvar = NULL;
		} else if (priority == 14 && cmp_skip(ctx, "?")) {
			variable *a = proceed_expression_internal(ctx, blk, isVector, priority, ef);
			cmp_err_skip(ctx, ":");
			variable *b = proceed_expression_internal(ctx, blk, isVector, priority, ef);
			if (ef) ret = ret ? a->intval : b->intval;
			retvar = NULL;
		} else {
			while (symbols[ctx->token->symbol].priority[1] == priority) {
				if ((i = (cmp_skip(ctx, "++"))) || cmp_skip(ctx, "--")) {
					if (ef) ret = i ? retvar->intval++ : retvar->intval--;
					retvar = NULL;
				} else if (cmp_skip(ctx, "(")) {
					long long args_val[16];
					int args_count = 0;
					do {
						variable *var = proceed_expression(ctx, blk, 1, ef);
						if (ef) { args_val[args_count++] = var->intval; }
					} while (cmp_skip(ctx, ","));
					cmp_err_skip(ctx, ")");
					if (ef) {
						block args = {ctx->global, NULL};
						token *tk = ctx->token;
						ctx->token = (token *) ret;
						i = 0;
						do {
							cmp_skip(ctx, "int"); cmp_skip(ctx, "char");
							if (ctx->token->type == T_IDENT) {
								variable *var = calloc(1, sizeof(variable));
								var->type = VT_INT;
								var->intval = args_val[i];
								args.table = map_add(args.table, ctx->token->text, var);
								ctx->token++;
							}
							i++;
						} while (cmp_skip(ctx, ","));
						cmp_err_skip(ctx, ")");
						proceed_statement(ctx, &args, 1);
						ret = ctx->return_value;
						retvar = NULL;
						ctx->token = tk;
					}
				} else if (cmp_skip(ctx, "[")) {
					variable *var = proceed_expression(ctx, blk, 0, ef);
					if (ef) retvar = (variable *)(ret + sizeof(variable) * var->intval);
					cmp_err_skip(ctx, "]");
				} else err("Not implemented: %s\n", ctx->token->text);
			}
		}
	}
	if (ef && retvar == NULL) {
		retvar = (variable *) calloc(1, sizeof(variable));
		retvar->type = VT_INT;
		retvar->intval = ret;
	}
	return retvar;
}

variable *proceed_expression(context *ctx, block *blk, int isVector, int ef) {
	return proceed_expression_internal(ctx, blk, isVector, 16, ef);
}

#define RTYPE_NORMAL    0
#define RTYPE_RETURN    1
#define RTYPE_BREAK     2
#define RTYPE_CONTINUE  3

int proceed_statement(context *ctx, block *parent, int ef) {
	int ret = RTYPE_NORMAL, i;
	variable *var = NULL;
	if (cmp_skip(ctx, "print")) {
		var = proceed_expression(ctx, parent, 0, ef);
		if (ef) printf("%lld\n", var->intval);
		cmp_err_skip(ctx, ";");
	} else if (cmp_skip(ctx, "puts")) {
		var = proceed_expression(ctx, parent, 0, ef);
		if (ef) {
			for (var = (variable *)var->intval; var->intval; var++)
				putchar(var->intval);
		}
		cmp_err_skip(ctx, ";");
	} else if (cmp_skip(ctx, "return")) {
		var = proceed_expression(ctx, parent, 0, ef);
		if (ef) ctx->return_value = var->intval;
		cmp_err_skip(ctx, ";");
		ret = RTYPE_RETURN;
	} else if (cmp_skip(ctx, "break")) {
		cmp_err_skip(ctx, ";"); ret = RTYPE_BREAK;
	} else if (cmp_skip(ctx, "continue")) {
		cmp_err_skip(ctx, ";"); ret = RTYPE_CONTINUE;
	} else if (cmp_skip(ctx, "int") || cmp_skip(ctx, "long") || cmp_skip(ctx, "char")) {
		do {
			cmp_skip(ctx, "long"); cmp_skip(ctx, "*");
			if (ef && (ctx->token->type != T_IDENT ||
				search(parent, ctx->token->text) != NULL))
				err("Identifier already used: %s\n", ctx->token->text);
			char *name = ctx->token->text;
			variable *arrlen = NULL, *var2 = NULL;
			ctx->token++;
			if (cmp_skip(ctx, "(")) {
				if (ef) {
					var = calloc(1, sizeof(variable));
					var->type = VT_FUNC;
					var->intval = (long long) ctx->token;
					parent->table = map_add(parent->table, name, var);
				}
				while (!cmp(ctx, ")") && ctx->token->type != T_NULL) ctx->token++;
				cmp_err_skip(ctx, ")");
				proceed_statement(ctx, parent, 0);
				return ret;
			} else {
				if (cmp_skip(ctx, "[")) {
					arrlen = proceed_expression(ctx, parent, 0, ef);
					cmp_err_skip(ctx, "]");
				}
				if (cmp_skip(ctx, "=")) var2 = proceed_expression(ctx, parent, 1, ef);
				if (ef) {
					var = calloc(1, sizeof(variable));
					if (arrlen != NULL) {
						var->intval = (long long) calloc(arrlen->intval, sizeof(variable));
						var->type = VT_ARRAY;
					} else {
						if (var2 != NULL) var->intval = var2->intval;
						var->type = VT_INT;
					}
					parent->table = map_add(parent->table, name, var);
				}
			}
		} while (cmp_skip(ctx, ","));
		cmp_err_skip(ctx, ";");
	} else if (cmp_skip(ctx, "if")) {
		cmp_err_skip(ctx, "(");
		var = proceed_expression(ctx, parent, 0, ef);
		cmp_err_skip(ctx, ")");
		int effective = 1;
		if (ef) effective = var->intval;
		i = proceed_statement(ctx, parent, ef && effective);
		if (ef && effective) ret = i;
		if (cmp_skip(ctx, "else")) {
			i = proceed_statement(ctx, parent, ef && !effective);
			if (ef && !effective) ret = i;
		}
	} else if (cmp_skip(ctx, "for")) {
		cmp_err_skip(ctx, "(");
		if (!cmp(ctx, ";")) proceed_expression(ctx, parent, 0, ef);
		cmp_err_skip(ctx, ";");
		token *start = ctx->token, *iterate, *end;
		do {
			if (!cmp(ctx, ";")) var = proceed_expression(ctx, parent, 0, ef);
			cmp_err_skip(ctx, ";");
			iterate = ctx->token;
			proceed_expression(ctx, parent, 0, 0);
			cmp_err_skip(ctx, ")");
			ret = proceed_statement(ctx, parent, ef = (ef && (var == NULL ||  var->intval)));
			end = ctx->token;
			ctx->token = iterate;
			if (!cmp(ctx, ")")) proceed_expression(ctx, parent, 0, ef);
			ctx->token = start;
		} while (ef && ret != RTYPE_RETURN && ret != RTYPE_BREAK);
		ctx->token = end;
		if (ret == RTYPE_BREAK) ret = RTYPE_NORMAL;
	} else if (cmp_skip(ctx, "while")) {
		cmp_err_skip(ctx, "(");
		token *start = ctx->token;
		do {
			ctx->token = start;
			var = proceed_expression(ctx, parent, 0, ef);
			cmp_err_skip(ctx, ")");
			ret = proceed_statement(ctx, parent, ef = (ef && var->intval));
		} while (ef && ret != RTYPE_RETURN && ret != RTYPE_BREAK);
		if (ret == RTYPE_BREAK) ret = RTYPE_NORMAL;
	} else if (cmp_skip(ctx, "do")) {
		token *start = ctx->token;
		do {
			ctx->token = start;
			ret = proceed_statement(ctx, parent, ef);
			cmp_err_skip(ctx, "while");
			cmp_err_skip(ctx, "(");
			var = proceed_expression(ctx, parent, 0, ef);
		} while (ef && var->intval && ret != RTYPE_RETURN && ret != RTYPE_BREAK);
		if (ret == RTYPE_BREAK) ret = RTYPE_NORMAL;
		cmp_err_skip(ctx, ")");
		cmp_err_skip(ctx, ";");
	} else if (cmp_skip(ctx, "{")) {
		block blk = {parent, NULL};
		while (!cmp_skip(ctx, "}")) {
			i = proceed_statement(ctx, &blk, ef);
			if (ef) { ret = i; ef = !ret; }
		}
	} else {
		if (!cmp(ctx, ";")) proceed_expression(ctx, parent, 0, ef);
		cmp_err_skip(ctx, ";");
	}
	return ret;
}

/* token構造体の配列を実行します */
int proceed(context *ctx) {
	int ef = 1, ret = RTYPE_NORMAL, i;
	block blk = {NULL, NULL};
	ctx->global = &blk;
	while (ctx->token->type != T_NULL) {
		i = proceed_statement(ctx, &blk, ef);
		if (ef) { ret = i; ef = !ret; }
	}
	return ctx->return_value;
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
		} else if (c == '"') {
			char str[1024], *s = str;
			while ((c = fgetc(fp)) != '"') {
				if (c == '\\') {
					c = fgetc(fp);
					if      (c == 'n') c = '\n';
					else if (c == 't') c = '\t';
				}
				*s++ = c;
			}
			*s++ = '\0';
			variable *var = calloc(strlen(str) + 1, sizeof(variable));
			tok[i].type = T_INTVAL;
			tok[i].intval = (long long) var;
			for (s = str; ; s++, var++) {
				var->type = VT_INT;
				var->intval = (long long) *s;
				if (!*s) break;
			}
			c = fgetc(fp);
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
				if (j == 0) err("Bad char: %c\n", s[0]);
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
		ctx.return_value = 0;
		if (fp != stdin) fclose(fp);
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
