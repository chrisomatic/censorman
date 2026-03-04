//===================================
// Chars
//===================================

b32 char_is_whitespace(u8 c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

b32 char_is_digit(u8 c)
{
    return (c >= '0' && c <= '9');
}

b32 char_is_alpha(u8 c)
{
    return (c >= 'A' && c <= 'z');
}

b32 char_is_lower(u8 c)
{
    return (c >= 'a' && c <= 'z');
}

b32 char_is_upper(u8 c)
{
    return (c >= 'A' && c <= 'Z');
}

u8 char_to_lower(u8 c)
{
    if(char_is_upper(c))
    {
        c += ('a' - 'A');
    }
    return c;
}

u8 char_to_upper(u8 c)
{
    if(char_is_lower(c))
    {
        c += ('A' - 'a');
    }
    return c;
}

//===================================
// C-Strings
//===================================

u64 cstring_strlen(const char *str)
{
    u64 len = 0;
    for(const char *p = str; *p; ++p) len++;
    return len;
}

//===================================
// Strings
//===================================

String string_format(Arena *arena, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    s32 required_len = stbsp_vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (required_len < 0)
    {
        return (String){ .len = 0, .data = NULL };
    }

    u8* buffer = PUSH_ARRAY(arena, u8, required_len);
    if (!buffer)
    {
        return (String){ .len = 0, .data = NULL };
    }

    va_start(args, format);
    stbsp_vsnprintf(buffer, required_len+1, format, args);
    va_end(args);

    return (String){ .len = (u32)required_len, .data = buffer };
}

char* string_to_cstr(Arena *arena, String str)
{
    char* cstr;
    cstr = PUSH_ARRAY(arena, char, str.len+1); // +1 for null terminator
    MemoryCopy(cstr,str.data, str.len);
    return cstr;
}

void string_print(String s)
{
    if(s.len == 0 || !s.data) return;
    logi(STR_FMT,s.len, s.data);
}

String string_nil()
{
    String str = {0};
    return str;
}

String string_copy(Arena *arena,String str)
{
    String ret = {0};
    ret.len = str.len;
    ret.data = PUSH_ARRAY(arena, u8, str.len);
    MemoryCopy(ret.data, str.data, str.len);

    return ret;
}

String string_concat(Arena *arena, u64 count, ...)
{
    va_list args1, args2;
    va_start(args1, count);
    va_copy(args2, args1);

    u64 total_len = 0;
    for(u64 i = 0; i < count; ++i)
    {
        String s = va_arg(args1, String);
        total_len += s.len;
    }
    va_end(args1);

    String str = {0};
    str.data = PUSH_ARRAY(arena, u8, total_len);

    for(u64 i = 0; i < count; ++i)
    {
        String s = va_arg(args2, String);
        memcpy(&str.data[str.len],s.data, s.len);
        str.len += s.len;
    }
    va_end(args2);

    return str;
}

String string_to_lower(Arena *arena, String str)
{
    String new_str = string_copy(arena, str);
    for(u64 i = 0; i < new_str.len; ++i)
    {
        new_str.data[i] = char_to_lower(new_str.data[i]);
    }
    return new_str;
}

String string_to_upper(Arena *arena, String str)
{
    String new_str = string_copy(arena, str);
    for(u64 i = 0; i < new_str.len; ++i)
    {
        new_str.data[i] = char_to_upper(new_str.data[i]);
    }
    return new_str;
}

u32 string_hash(String s)
{
    u32 hash = hash_data(s.data, s.len, 0x0);
    return hash;
}

b32 string_starts_with(String str, String start)
{
    if(start.len > str.len)
        return false;

    String prefix = string_substring(str, 0, start.len);
    return string_equal(prefix, start);
}

b32 string_ends_with(String str, String end)
{
    if (end.len > str.len)
        return false;

    String suffix = string_substring(str, str.len - end.len, end.len);
    return string_equal(suffix, end);
}

s64 string_get_first_index(String s, const char *find, b32 from_end)
{
    String find_str = STR(find);

    for(u64 i = from_end ? s.len - 1 : 0; from_end ? i >= 0 : i < s.len; i += from_end ? -1 : 1)
    {
        if(s.data[i] == find_str.data[0])
        {
            b32 match = true;
            for(u64 j = 1; j < find_str.len; ++j)
            {
                ++i;
                if(i >= s.len) {
                    match = false;
                    break;
                }

                if(s.data[i] != find_str.data[j])
                {
                    match = false;
                    break;
                }
            }

            if(match)
            {
                return i;
            }
        }
    }

    return -1;
}

b32 string_contains(String s, String find)
{
    if(find.len == 0) return false;

    for(u64 i = 0; i < s.len; ++i)
    {
        String sub = string_substring(s, i,find.len);
        if(string_equal(sub, find))
        {
            return true;
        }
    }

    return false;
}

String string_replace(Arena *arena, String str, String find, String replacement)
{
    if(find.len == 0)
    {
        // return a copy of the data
        return string_copy(arena, str);
    }

    // find number of instances
    u64 *instances = PUSH_ARRAY(arena, u64, str.len); // total possible in indices
    u32 instance_count = 0;

    for(u64 i = 0; i < str.len; ++i)
    {
        String sub = string_substring(str, i, find.len);
        if(string_equal(sub, find))
        {
            // found instance of 'find' string
            instances[instance_count++] = i;
        }
    }

    s64 new_len = str.len + ((replacement.len - find.len)*instance_count);
    if(new_len <= 0)
    {
        return string_nil();
    }

    // allocate space for new string
    String new_str = {0};
    new_str.len = (u64)new_len;
    new_str.data = PUSH_ARRAY(arena, u8, new_len);

    u64 str_index = 0;
    u64 new_index = 0;

    u32 i = 0;
    for(;;)
    {
        if(i >= instance_count)
        {
            // copy any remaining string after last instance
            MemoryCopy(new_str.data + new_index , str.data + str_index , str.len - str_index);
            break;
        }
        u64 instance_index = instances[i];
        MemoryCopy(new_str.data + new_index, str.data + str_index, instance_index - str_index);
        new_index += instance_index - str_index;
        str_index = instance_index;

        MemoryCopy(new_str.data + new_index, replacement.data, replacement.len);
        new_index += replacement.len;
        str_index += find.len;

        i++;
    }

    return new_str;
}

b32 string_in_list(String str, StringList list)
{
    StringNode *sn = list.head;
    for(;;)
    {
        if(string_equal(str, sn->str))
            return true;

        if(!sn->next) break;
        sn = sn->next;
    }
    return false;
}

b32 string_in_array(String str, StringArray arr)
{
    for(u64 i = 0; i < arr.count; ++i)
    {
        if(string_equal(str, arr.items[i]))
            return true;
    }
    return false;
}


b32 string_equal(String s, String t)
{
    if(s.len != t.len) return false;

    for(u64 i = 0; i < s.len; ++i)
        if(s.data[i] != t.data[i])
            return false;

    return true;
}


String string_substring(String s, u64 start, u64 len)
{
    String ret = {
        .data = s.data + start,
        .len = len
    };
    return ret;
}

String string_rtrim(String s)
{
    for(u8 *p = s.data+s.len-1; s.len > 0 && char_is_whitespace(*p); --p, s.len--);
    return s;
}

String string_ltrim(String s)
{
    for(u8 *p = s.data; s.len > 0 && char_is_whitespace(*p); ++p, s.data++, s.len--);
    return s;
}

String string_trim(String s)
{
    s = string_rtrim(s);
    s = string_ltrim(s);
    return s;
}

////////////////////////////////////
// String Conversions

f64 string_to_f64(String str)
{
    f64 f = 0.0;
    s32 e = 0;
    s32 c;
    s32 sign = 1;

    str = string_eat_whitespace(str);

    while(string_top_char(str) == '-')
    {
        str = string_advance_char(str);
        sign *= -1;
    }

    for(;;)
    {
        c = string_get_char_and_advance(&str);

        if(!char_is_digit(c)) break;

        f = f*10.0 + (c - '0');
    }

    if (c == '.')
    {
        for(;;)
        {
            c = string_get_char_and_advance(&str);
            if(!char_is_digit(c)) break;

            f = f*10.0 + (c - '0');
            e = e-1;
        }
    }

    if (c == 'e' || c == 'E')
    {
        s32 sign = 1;
        s32 i = 0;

        c = string_get_char_and_advance(&str);

        if (c == '+')
        {
            c = string_get_char_and_advance(&str);
        }
        else if (c == '-')
        {
            c = string_get_char_and_advance(&str);
            sign = -1;
        }

        for(;;)
        {
            if(!char_is_digit(c)) break;
            i = i*10 + (c - '0');
            c = string_get_char_and_advance(&str);
        }

        e += i*sign;
    }

    while (e > 0)
    {
        f *= 10.0;
        e--;
    }

    while (e < 0)
    {
        f *= 0.1;
        e++;
    }

    f *= sign;

    return f;
}

s64 string_to_s64(String str)
{
    s64 i = 0;
    s32 e = 0;
    s32 c = 0;
    s32 sign = 1;
    str = string_eat_whitespace(str);

    for(;;)
    {
        if(string_top_char(str) != '-')
            break;

        str = string_advance_char(str);
        sign *= -1;
    }

    for(;;)
    {
        u8 c = string_get_char_and_advance(&str);
        if(!char_is_digit(c)) break;

        i = i*10 + (c - '0');
    }

    if (c == 'e' || c == 'E')
    {
        s32 sign = 1;
        s32 n = 0;

        c = string_get_char_and_advance(&str);

        if (c == '+')
        {
            c = string_get_char_and_advance(&str);
        }
        else if (c == '-')
        {
            c = string_get_char_and_advance(&str);
            sign = -1;
        }

        for(;;)
        {
            if(!char_is_digit(c)) break;
            n = n*10 + (c - '0');
            c = string_get_char_and_advance(&str);
        }

        e += n*sign;
    }

    while (e > 0)
    {
        i *= 10.0;
        e--;
    }

    while (e < 0)
    {
        i *= 0.1;
        e++;
    }

    i *= sign;

    return i;
}

////////////////////////////////////
// String Lists

StringList string_list_create(Arena *arena)
{
    StringList sl = {0};

    sl.head = NULL;
    sl.last = NULL;
    sl.count = 0;
    sl.arena = arena;

    return sl;
}

void string_list_add(StringList *sl, String str)
{
    if(sl->head == NULL)
    {
        // first element
        sl->head = PUSH_ONE(sl->arena, StringNode);
        sl->head->prev = NULL;
        sl->head->next = NULL;
        sl->last = sl->head;
    }
    else
    {
        StringNode *prior_last = sl->last;
        sl->last->next = PUSH_ONE(sl->arena, StringNode);
        sl->last = sl->last->next;
        sl->last->next = NULL;
        sl->last->prev = prior_last;
    }

    sl->last->str.data = str.data;
    sl->last->str.len = str.len;

    sl->count++;
}

void string_list_addf(StringList *sl, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    s32 required_len = stbsp_vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (required_len <= 0)
    {
        return;
    }

    u8 *buffer = PUSH_ARRAY(sl->arena, u8, required_len);
    if (!buffer)
    {
        return;
    }

    va_start(args, format);
    stbsp_vsnprintf(buffer, required_len+1, format, args);
    va_end(args);

    String str = {(u32)required_len, buffer};

    string_list_add(sl, str);

    return;

}

b32 string_list_remove(StringList *sl, u64 index)
{
    StringNode *ln = sl->head;
    for(u64 idx = 0; ln && idx < index; ++idx)
        ln = ln->next;

    if(!ln) return false;

    if(ln->prev) ln->prev->next = ln->next;
    else         sl->head     = ln->next;

    if(ln->next) ln->next->prev = ln->prev;
    else         sl->last     = ln->prev;

    ln->prev = NULL;
    ln->next = NULL;
    sl->count--;

    return true;
}

String string_list_get(StringList *sl, u64 index)
{
    if(index >= sl->count)
        return string_nil();

    u64 idx = 0;
    StringNode *sn = sl->head;

    for(;;)
    {
        if(idx > index)
            break;

        if(idx == index)
            return sn->str;

        if(!sn->next)
            break;

        sn = sn->next;
        idx++;
    }

    return string_nil();
}

String string_list_collapse(StringList *sl)
{
    String str = {0};
    u64 total_size = 0;

    StringNode *sn;
   
    sn = sl->head;
    if(sn == NULL)
    {
        logw("StringNode is null");
        return str;
    }

    for(;;)
    {
        total_size += sn->str.len;

        if(!sn->next)
            break;

        sn = sn->next;
    }

    if(total_size == 0)
        return str;

    str.data = PUSH_ARRAY(sl->arena, u8, total_size);

    sn = sl->head;
    for(;;)
    {
        String *it = &sn->str;
        memcpy(&str.data[str.len], it->data, it->len);
        str.len += it->len;

        if(!sn->next)
            break;
        sn = sn->next;
    }

    return str;
}

String string_eat_whitespace(String str)
{
    String ret = str;

    for(;;)
    {
        if(ret.len == 0) break;
        if(!char_is_whitespace(ret.data[0])) break;
        ret.data += sizeof(u8);
        ret.len--;
    }

    return ret;
}

void string_advance_in_place(String *str, u64 count)
{
    if(str->len < count) return;

    str->len -= count;
    str->data += count;
}

String string_advance(String str, u64 count)
{
    String ret = {0};
    if(str.len < count) return ret;

    ret.len = str.len - count;
    ret.data = str.data + count;
    return ret;
}

u8 string_get_char_at_index(String str, u64 index)
{
    u8 c = str.len > 0 ? str.data[index] : '\0';
    return c;
}

u8 string_get_char_and_advance(String *str)
{
    u8 c = str->len > 0 ? str->data[0] : '\0';
    string_advance_in_place(str, 1);
    return c;
}

u8 string_top_char(String str)
{
    return string_get_char_at_index(str,0);
}

String string_advance_char(String str)
{
    return string_advance(str, 1);
}

StringArray string_array_nil()
{
    StringArray sa = {0};
    return sa;
}

StringArray string_array_create(Arena *arena, u64 count, ...)
{
    StringArray sa;
    sa.count = count;
    sa.items = PUSH_ARRAY(arena, String, count);

    va_list args;
    va_start(args, count);

    for(u64 i = 0; i < count; ++i)
    {
        sa.items[i] = va_arg(args, String);
    }

    va_end(args);
    return sa;
}

StringArray string_array_create_empty(Arena *arena, u64 count)
{
    StringArray sa;
    sa.count = count;
    sa.items = PUSH_ARRAY(arena, String, count);
    return sa;
}

StringArray string_list_to_array(StringList sl)
{
    StringArray sa;
    sa.count = sl.count;
    sa.items = PUSH_ARRAY(sl.arena, String, sl.count);

    StringNode *sn = sl.head;
    if(!sn) return sa;

    for(u64 i = 0; i < sl.count; ++i)
    {
        MemoryCopy(&sa.items[i], &sn->str, sizeof(String));
        if(!sn->next) break;
        sn = sn->next;
    }

    return sa;
}

StringArray string_split(Arena *arena, String base, String split)
{
    u64 num_strings = base.len > 0 ? 1 : 0;
    s64 split_indices[2048] = {0}; // @HARDCODED @NOTE: Arbitrary limit

    for(u64 i = 0; i < base.len; ++i)
    {
        if(base.data[i] == split.data[0])
        {
            b32 match = true;
            for(u64 j = 1; j < split.len; ++j)
            {
                ++i;
                if(i >= base.len) {
                    match = false;
                    break;
                }

                if(base.data[i] != split.data[j])
                {
                    match = false;
                    break;
                }
            }

            if(match)
            {
                split_indices[num_strings] = i+1;
                num_strings++;
            }
        }
    }

    split_indices[num_strings] = base.len;

    StringArray sa = {0};
    sa.items = PUSH_ARRAY(arena, String, num_strings);
    sa.count = num_strings;

    for(u64 i = 0; i < num_strings; ++i)
    {
        u64 string_len = split_indices[i+1] - split_indices[i] - split.len;
        if(i+1 == num_strings) string_len += split.len;

        String *str = &sa.items[i];
        str->data = PUSH_ARRAY(arena, u8, string_len);
        str->len = string_len;

        memcpy(str->data, &base.data[split_indices[i]], string_len);
    }

    return sa;
}
