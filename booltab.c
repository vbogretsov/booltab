#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include <commander/commander.h>

typedef struct
{
    char name;
    int value;
} var_t;

#define PX_TOKEN_VALUE var_t var;

#include <postfix/postfix.h>

enum
{
    SUCCESS,
    E_MISSING_EXPRESSION,
    E_UNEXPECTED_ARG,
    E_UNEXPECTED_TOKEN,
};

#define VERSION    "1.0.0"
#define NAME       "booltab"
#define USAGE      "EXPRESSION\n\n" \
"  Calculate the boolean expressison for all possible values of its variables.\n" \
"  \n" \
"  Arguments:\n\n" \
"    EXPRESSION                    boolean expression to be calcluated, valid\n" \
"                                  tokens are: '(', ')', '~', '&', '|'\n" \
"                                  variable names should be sequential letters\n" \
"                                  in lower case [a-z], for example expression\n" \
"                                  '(a | ~b) & c' is valid but\n"\
"                                  '(a | ~b) & d' is invalid because the\n" \
"                                  variable 'd' cannot appear in expression if\n" \
"                                  the variable 'c' is missing"

#define MSG_E "error: "
#define MSG_E_MISSING_EXPRESSION "missing argument"
#define MSG_E_UNEXPECTED_ARG "unexpected argument"
#define MSG_E_UNEXPECTED_TOKEN "unexpected token"
#define MSG_E_UNKNOWN_ERRPR "unknown error"
#define MSG_PX_E_UNMATCHED_BRACKET "unmatched bracket detected"
#define MSG_PX_E_STAK_OVERFLOW "expression is too big, exeeded maximum stack size"
#define MSG_PX_E_MISSING_ARGUMENT "missing arument"
#define MSG_PX_E_STACK_CORRUPTED "expression is invalid, stack corrupted"

#define MAX_TOKENS 1 << 10

typedef uint64_t vartab_t;

#define MAX_VARS 'z' - 'a'

#define BIT(n, k) ((n) & ( 1 << (k) )) >> (k)
#define ISVAR(c)  ('a' <= (c) && c <= 'z')
#define VARNUM(c) (c) - 'a'

enum
{
    PX_BOOL_OR = PX_TOKEN_RBRC + 1,
    PX_BOOL_AND,
    PX_BOOL_NOT,
};

static int valueof(char name, void* ctx)
{
    int num = VARNUM(name);
    return (int)BIT(*(vartab_t*)ctx, num);
}

#define POP_ARGUMENT(bp, sp, ctx, arg)                                        \
if (*(sp) == (bp))                                                            \
{                                                                             \
    return PX_E_MISSING_ARGUMENT;                                             \
}                                                                             \
var_t arg = PX_STACK_POP(*(sp)).var;                                          \
if (ISVAR((arg).name))                                                        \
{                                                                             \
    (arg).value = valueof((arg).name, (ctx));                                 \
}

static int _not(px_value_t* bp, px_value_t** sp, void* ctx)
{
    POP_ARGUMENT(bp, sp, ctx, a);

    var_t res = (var_t){.value = !a.value, .name = 0};

    PX_STACK_PUSH(*sp, (px_value_t){.var = res});
    return PX_SUCCESS;
}

static int _and(px_value_t* bp, px_value_t** sp, void* ctx)
{
    POP_ARGUMENT(bp, sp, ctx, a);
    POP_ARGUMENT(bp, sp, ctx, b);

    var_t res = (var_t){.value = b.value && a.value, .name = 0};

    PX_STACK_PUSH(*sp, (px_value_t){.var = res});
    return PX_SUCCESS;
}

static int _or(px_value_t* bp, px_value_t** sp, void* ctx)
{
    POP_ARGUMENT(bp, sp, ctx, a);
    POP_ARGUMENT(bp, sp, ctx, b);

    var_t res = (var_t){.value = b.value || a.value, .name = 0};

    PX_STACK_PUSH(*sp, (px_value_t){.var = res});
    return PX_SUCCESS;
}

static int BOO_PRIO[] =
{
    0, // PX_TOKEN_TERM
    0, // PX_TOKEN_VAR
    0, // PX_TOKEN_LBRC
    0, // PX_TOKEN_RBRC
    1, // PX_BOOL_OR
    2, // PX_BOOL_AND
    3, // PX_BOOL_NOT
};

static int _prio(px_token_t t)
{
    return BOO_PRIO[t.type];
}

static px_token_t getvar(char name)
{
    var_t var = (var_t){.name = name};
    return (px_token_t){.type = PX_TOKEN_VAR, .value.var = var};
}

static int getnumvars(px_token_t* infix)
{
    int res = 0;
    bool vars[MAX_VARS];

    for (int i = 0; i < PX_LEN(vars); ++i)
    {
        vars[i] = true;
    }

    for (int i = 0; infix[i].type != PX_TOKEN_TERM; ++i)
    {
        int n = VARNUM(infix[i].value.var.name);
        if (infix[i].type == PX_TOKEN_VAR && vars[n])
        {
            ++res;
            vars[n] = false;
        }
    }

    return res;
}

static void printvars(vartab_t vars, int nvars)
{
    for (int i = 0; i < nvars; ++i)
    {
        printf("%d ", (int)BIT(vars, i));
    }
}

static int tokenize(char* expr, px_token_t* infix, void** einfo)
{
    int tok = 0;
    for (char* p = expr; *p != '\0'; ++p)
    {
        switch (*p)
        {
        case '(':
            infix[tok++] = (px_token_t){.type = PX_TOKEN_LBRC};
            break;
        case ')':
            infix[tok++] = (px_token_t){.type = PX_TOKEN_RBRC};
            break;
        case '|':
            infix[tok++] = (px_token_t){.type = PX_BOOL_OR, .value.op = _or};
            break;
        case '&':
            infix[tok++] = (px_token_t){.type = PX_BOOL_AND, .value.op = _and};
            break;
        case '~':
            infix[tok++] = (px_token_t){.type = PX_BOOL_NOT, .value.op = _not};
            break;
        case ' ':
            break;
        default:
            if (!ISVAR(*p))
            {
                *einfo = p;
                return E_UNEXPECTED_TOKEN;
            }

            infix[tok++] = getvar(*p);
            break;
        }
    }
    infix[tok] = (px_token_t){.type = PX_TOKEN_TERM};
    return 0;
}

static int calculate(px_token_t* infix, void** einfo)
{
    int err = PX_SUCCESS;

    px_token_t postfix[MAX_TOKENS];
    if (PX_SUCCESS != (err = px_parse(infix, postfix, _prio)))
    {
        return err;
    }

    int nvars = getnumvars(infix);
    int nlines = 1 << nvars;

    for (vartab_t i = 0; i < nlines; ++i)
    {
        px_value_t res;
        if (PX_SUCCESS != (err = px_eval(postfix, &i, &res)))
        {
            return err;
        }

        printvars(i, nvars);
        printf("%d\n", res.var.value);
    }

    return err;
}

static int run(command_t* cmd)
{
    if (0 == cmd->argc)
    {
        fprintf(stderr, "%s %s %s\n", MSG_E, MSG_E_MISSING_EXPRESSION, USAGE);
        return E_MISSING_EXPRESSION;
    }

    if (cmd->argc > 1)
    {
        const char* arg = cmd->argv[1];
        fprintf(stderr, "%s %s %s\n", MSG_E, MSG_E_UNEXPECTED_ARG, arg);
        return E_UNEXPECTED_ARG;
    }

    int err;
    void* einfo;

    px_token_t infix[MAX_TOKENS];
    if (SUCCESS != (err = tokenize(cmd->argv[0], infix, &einfo)))
    {
        fprintf(
            stderr,
            "%s %s '%c'\n",
            MSG_E,
            MSG_E_UNEXPECTED_TOKEN,
            *(char*)einfo);
        return err;
    }

    err = calculate(infix, &einfo);

    switch (err)
    {
    case PX_SUCCESS:
        break;
    case PX_E_UNMATCHED_BRACKET:
        fprintf(stderr, "%s %s\n", MSG_E, MSG_PX_E_UNMATCHED_BRACKET);
        break;
    case PX_E_STAK_OVERFLOW:
        fprintf(stderr, "%s %s\n", MSG_E, MSG_PX_E_STAK_OVERFLOW);
        break;
    case PX_E_MISSING_ARGUMENT:
        fprintf(stderr, "%s %s\n", MSG_E, MSG_PX_E_MISSING_ARGUMENT);
        break;
    case PX_E_STACK_CORRUPTED:
        fprintf(stderr, "%s %s\n", MSG_E, MSG_PX_E_STACK_CORRUPTED);
        break;
    default:
        fprintf(stderr, "%s %s\n", MSG_E, MSG_E_UNKNOWN_ERRPR);
        break;
    }

    return err;
}

int main(int argc, char *argv[])
{
    command_t cmd;
    command_init(&cmd, NAME, VERSION);
    cmd.usage = USAGE;
    command_parse(&cmd, argc, argv);

    int err = run(&cmd);

    command_free(&cmd);
    return err;
}
