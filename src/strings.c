char escapeToChar[256] = {
    ['\''] = '\'',
    ['"'] = '"',
    ['`'] = '`',
    ['\\'] = '\\',
    ['n'] = '\n',
    ['r'] = '\r',
    ['t'] = '\t',
    ['v'] = '\v',
    ['b'] = '\b',
    ['a'] = '\a',
    ['0'] = 0,
};

u32 DecodeCodePoint(u32 *cpLen, const char *str) {
    static const u32 FIRST_LEN[] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 4, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1
    };

    static const u8 MASK[] = {
        0xFF, 0xFF, 0x1F, 0xF, 0x7
    };

    u8 b0 = str[0];
    i32 l = FIRST_LEN[b0];
    i32 val = (i32)(b0 & MASK[l]);

    for (i32 i=1; i < l; i += 1) {
        val = (val << 6) | (i32)(str[i] & 0x3f);
    }

    if (cpLen)
        *cpLen = l;
    return val;
}

const char *Unescape(const char *str) {
    u32 len = 0;
    b32 sawSlash = false;

    for (size_t i=0; str[i] != '\0'; i += 1, len += 1) {
        if (str[i] == '\\') {
            sawSlash = true;
        }
    }

    if (!sawSlash)
        return str;

    char *newStr = calloc(len+1, sizeof(char));
    u32 newLen = 0;

    u32 i = 0;
    while (i < len) {
        u32 cpLen;
        u32 cp = DecodeCodePoint(&cpLen, str+i);
        if (cp == '\\') {
            // FIXME(Brett): error handling
            cp = escapeToChar[(u8)DecodeCodePoint(&cpLen, str+(i++))];
            cpLen = 1;
        }

        memcpy(newStr+newLen, str+i, cpLen);

        i += cpLen;
        newLen += cpLen;
    }

    return newStr;
}
