#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dmcp.h>

typedef struct
{
    const int key;
    const char value;
    const char shift;
} key_t;

static key_t alpha_map[] = {
    {KEY_SIGMA, 'a', '('},
    {KEY_INV, 'b', ')'},
    {KEY_SQRT, 'c', '['},
    {KEY_LOG, 'd', ']'},
    {KEY_LN, 'e', '{'},
    {KEY_XEQ, 'f', '}'},
    {KEY_STO, 'g', '$'},
    {KEY_RCL, 'h', '%'},
    {KEY_RDN, 'i', '"'},
    {KEY_SIN, 'j', '\''},
    {KEY_COS, 'k', '&'},
    {KEY_TAN, 'l', ','},
    {KEY_SWAP, 'm', '@'},
    {KEY_CHS, 'n', '!'},
    {KEY_E, 'o', ';'},
    {KEY_7, 'p', '7'},
    {KEY_8, 'q', '8'},
    {KEY_9, 'r', '9'},
    {KEY_DIV, 's', '/'},
    {KEY_4, 't', '4'},
    {KEY_5, 'u', '5'},
    {KEY_6, 'v', '6'},
    {KEY_MUL, 'w', '*'},
    {KEY_1, 'x', '1'},
    {KEY_2, 'y', '2'},
    {KEY_3, 'z', '3'},
    {KEY_SUB, '-', '_'},
    {KEY_0, ':', '0'},
    {KEY_DOT, '.', 0},
    {KEY_RUN, '?', 0},
    {KEY_ADD, ' ', '+'},
    {0, 0, 0},
};

char handle_alpha(int key, bool shift, int upper)
{
    char ch = 0;
    for (key_t *p = &alpha_map[0]; p->key != 0; p++)
    {
        if (p->key == key)
        {
            if (shift)
            {
                ch = p->shift;
            }
            else
            {
                ch = p->value;
            }
            break;
        }
    }
    if (upper && ch >= 'a' && ch <= 'z')
        ch += ('A' - 'a');
    return ch;
}
