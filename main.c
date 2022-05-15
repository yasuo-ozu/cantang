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
	"void", "char", "signed", "unsigned", "long", "const",
	NULL
};

struct {
	const char * const text;
	int priority[3];	// PRE, POST, BINARY
} symbols[] = {
	{NULL, {0,0,0}}, {"++", {2, 1, 0}}, {"--", {2, 1, 0}}, {"->", {0, 0, 1}}, {".", {0, 0, 1}}, {"~", {2, 0, 0}},
	{"!", {2, 0, 0}}, {"*", {2, 0, 4}}, {"/", {0, 0, 4}}, {"%", {0, 0, 4}}, {"+", {2, 0, 5}}, {"-", {2, 0, 5}},
	{"<<", {0, 0, 6}}, {">>", {0, 0, 6}}, {"<", {0, 0, 7}}, {"<=", {0, 0, 7}}, {">", {0, 0, 7}}, {">=", {0, 0, 7}},
	{"==", {0, 0, 8}}, {"!=", {0, 0, 8}}, {"&", {2, 0, 9}}, {"^", {0, 0, 10}}, {"|", { 0,  0, 11}}, {"&&", {0, 0, 12}},
	{"||", {0, 0, 13}}, {"=", {0, 0, 15}}, {"+=", {0, 0, 15}}, {"-=", {0, 0, 15}}, {"*=", {0, 0, 15}},
	{"/=", {0, 0, 15}}, {"%=", {0, 0, 15}}, {"<<=", {0, 0, 15}}, {">>=", {0, 0, 15}}, {"&=", {0, 0, 15}},
	{"^=", {0, 0, 15}}, {"|=", {0, 0, 15}}, {",", {0, 0, 16}}, {"(", {0, 1, 0}}, {")", {0, 0, 0}}, {"{", {0, 0, 0}},
	{"}", {0, 0, 0}}, {"[", {0, 1, 0}}, {"]", {0, 0, 0}}, {"?", {0, 0, 0}}, {":", {0, 0, 0}}, {";", {0, 0, 0}},
	{NULL,  { 0,  0,  0}}
};

typedef struct map {
	struct map *next;
	char *key;
	void *value;
} map;

typedef struct {
	enum {
		VT_NULL = 0, VT_INT, VT_FUNC, VT_ARRAY, VT_STRUCT
	} type;
	long long intval;
	map *table;		// for struct
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

map *map_add(map *m, char *key, void *value) {
	map *n = malloc(sizeof(map));
	n->next = m;
	n->key = key;
	n->value = value;
	return n;
}

int map_count(map *m) {
	if (m == NULL) return 0;
	return 1 + map_count(m->next);
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

int type_cmp_skip(context *ctx) {
	int ret = 0;
	while (1) {
		if (cmp_skip(ctx, "int") || cmp_skip(ctx, "void") || cmp_skip(ctx, "char")
			|| cmp_skip(ctx, "signed") || cmp_skip(ctx, "unsigned") || cmp_skip(ctx, "long")
			|| cmp_skip(ctx, "const") || cmp_skip(ctx, "*")) ret++;
		else if (cmp_skip(ctx, "struct")) ctx->token++, ret++;
		else break;
	}
	return ret;
}

int proceed_statement(context *, block *, int);
variable *proceed_expression(context *, block *, int, int);
variable *proceed_expression_internal(context *ctx, block *blk, int isVector, int priority, int ef) {
	variable *retvar = NULL;
	long long ret = 0, i = 0;
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
		if (strcmp(op->text, "*") == 0) retvar = (variable *) ret; 
		else if (strcmp(op->text, "&") == 0) {
			variable *var = calloc(1, sizeof(variable));
			var->type = VT_INT;
			var->intval = (long long) retvar;
			retvar = var;
		} else {
			if      (strcmp(op->text, "+") == 0) ret = +ret;
			else if (strcmp(op->text, "-") == 0) ret = -ret;
			else if (strcmp(op->text, "!") == 0) ret = !ret;
			else if (strcmp(op->text, "~") == 0) ret = ~ret;
			else if ((i = (strcmp(op->text, "++") == 0)) || strcmp(op->text, "--") == 0) {
				if (ef) ret = i ? ++retvar->intval : --retvar->intval;
			} else err("Not implemented: %s\n", ctx->token->text);
			retvar = NULL;	// You have to duplicate variable
		}
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
				retvar = NULL;
			} else if ((i = cmp_skip(ctx, "->")) || cmp_skip(ctx, ".")) {
				if (ef) {
					if (i) retvar = (variable *)retvar->intval;
					map *m = retvar->table;
					for (i = 0; m != NULL; m = m->next, i++)
						if (strcmp(m->key, ctx->token->text) == 0) break;
					if (m == NULL) err("Invalid member: %s\n", ctx->token->text);
					retvar += i;
				}
				ctx->token++;
			} else {
				do {
					int effective = 1;
					if (cmp(ctx, "&&")) effective = ret;
					else if (cmp(ctx, "||")) effective = !ret;
					op = ctx->token++;
					variable *right = proceed_expression_internal(ctx, blk, isVector, priority - 1, ef && effective);
					if (ef && effective) ret = proceed_binary_operator(op, ret, right->intval);
				} while (symbols[ctx->token->symbol].priority[2] == priority);
				retvar = NULL;
			}
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
					variable *args_val[16];
					int args_count = 0;
					if (!cmp(ctx, ")")) {
						do {
							variable *var = proceed_expression(ctx, blk, 1, ef);
							if (ef) {
								int size = var->table != NULL ? map_count(var->table) : 1;
								variable *var2 = calloc(size, sizeof(variable));
								memcpy(var2, var, size * sizeof(variable));
								args_val[args_count++] = var2;
							}
						} while (cmp_skip(ctx, ","));
					}
					cmp_err_skip(ctx, ")");
					if (ef) {
						block args = {ctx->global, NULL};
						token *tk = ctx->token;
						ctx->token = (token *) ret;
						i = 0;
						do {
							type_cmp_skip(ctx);
							if (ctx->token->type == T_IDENT) {
								args.table = map_add(args.table, ctx->token->text, args_val[i]);
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
					if (ef) {
						retvar = (variable *)(ret + sizeof(variable) * var->intval);
						ret = retvar->intval;
					}
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

variable *allocate_array_mem(variable *arrlens[], int max, int index) {
	variable *var, *var2;
	int len = arrlens[index]->intval, i;
	if (index == max - 1) {
		var = calloc(len, sizeof(variable));
		var->type = VT_ARRAY;
	} else {
		var = calloc(len, sizeof(variable));
		for (i = 0; i < len; i++) {
			var2 = allocate_array_mem(arrlens, max, index + 1);
			var[i].intval = (long long) var2;
			var[i].type = VT_ARRAY;
		}
	}
	return var;
}

int proceed_statement(context *ctx, block *parent, int ef) {
	int ret = RTYPE_NORMAL, i = 0;
	variable *var = NULL;
	if (cmp_skip(ctx, "if")) {
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
	} else if (cmp_skip(ctx, "{")) {
		block blk = {parent, NULL};
		while (!cmp_skip(ctx, "}")) {
			i = proceed_statement(ctx, &blk, ef);
			if (ef) { ret = i; ef = !ret; }
		}
	} else {
		if (cmp_skip(ctx, "print")) {
			var = proceed_expression(ctx, parent, 0, ef);
			if (ef) printf("%lld\n", var->intval);
		} else if (cmp_skip(ctx, "puts")) {
			var = proceed_expression(ctx, parent, 0, ef);
			if (ef) {
				for (var = (variable *)var->intval; var->intval; var++)
					putchar(var->intval);
			}
		} else if (cmp_skip(ctx, "return")) {
			var = proceed_expression(ctx, parent, 0, ef);
			if (ef) ctx->return_value = var->intval;
			ret = RTYPE_RETURN;
		} else if ((i = cmp_skip(ctx, "break")) || cmp_skip(ctx, "continue")) {
			ret = i ? RTYPE_BREAK : RTYPE_CONTINUE;
		} else if (cmp_skip(ctx, "struct")) {
			if (ctx->token->type != T_IDENT) err("struct name is invalid");
			if ((var = search(parent, ctx->token->text)) != NULL) {
				ctx->token++;
				do {
					if (ef) {
						variable *var2 = calloc(map_count(var->table), sizeof(variable));
						var2->table = var->table;
						parent->table = map_add(parent->table, ctx->token->text, var2);
					}
					ctx->token++;
				} while (cmp_skip(ctx, ","));
			} else {
				if (ef) {
					var = calloc(1, sizeof(variable));
					var->type = VT_STRUCT;
					parent->table = map_add(parent->table, ctx->token->text, var);
				}
				ctx->token++;
				cmp_err_skip(ctx, "{");
				while (type_cmp_skip(ctx)) {
					do {
						if (ef) var->table = map_add(var->table, ctx->token->text, NULL);
						ctx->token++;
					} while (cmp_skip(ctx, ","));
					cmp_err_skip(ctx, ";");
				}
				cmp_err_skip(ctx, "}");
			}
		} else if (type_cmp_skip(ctx)) {
			do {
				if (ef && (ctx->token->type != T_IDENT ||
					map_search(parent->table, ctx->token->text) != NULL))
					err("Identifier already used: %s\n", ctx->token->text);
				char *name = ctx->token->text;
				variable *arrlens[16], *var2 = NULL;
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
					i = 0;
					if (cmp_skip(ctx, "[")) {
						do {
							arrlens[i++] = proceed_expression(ctx, parent, 0, ef);
							cmp_err_skip(ctx, "]");
						} while (cmp_skip(ctx, "["));
					}
					if (cmp_skip(ctx, "=")) var2 = proceed_expression(ctx, parent, 1, ef);
					if (ef) {
						var = calloc(1, sizeof(variable));
						if (i > 0) {
							var->intval = (long long) allocate_array_mem(arrlens, i, 0);
							var->type = VT_ARRAY;
						} else {
							if (var2 != NULL) var->intval = var2->intval;
							var->type = VT_INT;
						}
						parent->table = map_add(parent->table, name, var);
					}
				}
			} while (cmp_skip(ctx, ","));
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
		} else {
			if (!cmp(ctx, ";")) proceed_expression(ctx, parent, 0, ef);
		}
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
	token *tok = calloc(1024 * 32, sizeof(token));
	int i = 0;
	int c = fgetc(fp);
	while (1) {
		char str[1024], *s = str;
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
				} else if (c == '/') {
					while (c != '\n' && c != EOF) c = fgetc(fp);
				} else {
					ungetc(c, fp);
					c = '/';
					break;
				}
			} else break;
		}
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
		} else if (c == '\'') {
			long long d = 0;
			while ((c = fgetc(fp)) != '\'') {
				if (c == '\\') {
					c = fgetc(fp);
					if      (c == 'n') c = '\n';
					else if (c == 't') c = '\t';
				}
				*s++ = c;
				d = (d << 8) + c;
			}
			*s++ = 0;
			tok[i].type = T_INTVAL;
			tok[i].intval = (long long) d;
			c = fgetc(fp);
		} else if (c == '"') {
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
			int j = 0;
			if (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || c == '_') {
				do {
					*s++ = (char) c;
					c = fgetc(fp);
				} while (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') 
						|| c == '_' || ('0' <= c && c <= '9'));
				for (*s = '\0'; keywords[j] != NULL; j++) {
					if (strcmp(keywords[j], str) == 0) break;
				}
				tok[i].type = keywords[j] == NULL ? T_IDENT : T_KEYWORD;
			} else {
				tok[i].type = T_SYMBOL;
				while (c != EOF) {
					*s++ = (char) c; *s = '\0';
					for (j = 1; symbols[j].text != NULL; j++) {
						if (strcmp(symbols[j].text, str) == 0) break;
					}
					if (symbols[j].text == NULL) {
						*--s = '\0';
						break;
					}
					tok[i].symbol = j;
					c = fgetc(fp);
				}
				if (j == 0) err("Bad char: %c\n", str[0]);
			}
			tok[i].text = strdup(str);
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
