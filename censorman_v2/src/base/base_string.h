#pragma once

#define S(literal)      (String){sizeof(literal)-1, (u8*)(literal)}
#define STR(cstr)       (String){cstring_strlen(cstr),(u8*)(cstr)}

#define STR_EMPTY(x)    ((x) == 0 || cstring_strlen(x) == 0)
#define STR_BOOL(b)     ((b) ? "True" : "False")

// Used for printing String type in printf-like functions
#define STR_FMT "%.*s"
#define STR_ARG(s) s.len, s.data

typedef struct
{
    u64 len;
    u8* data;
} String;

typedef struct
{
    u64 count;
    String *items;
} StringArray;

typedef struct StringNode StringNode;
struct StringNode
{
    String str;
    StringNode *prev;
    StringNode *next;
};

typedef struct
{
    StringNode *head;
    StringNode *last;
    u64 count;
    Arena *arena;
} StringList;

///////////////////////////
// Chars
///////////////////////////

b32 char_is_whitespace(u8 c);
b32 char_is_digit(u8 c);
b32 char_is_alpha(u8 c);
b32 char_is_lower(u8 c);
b32 char_is_upper(u8 c);
u8  char_to_lower(u8 c);
u8  char_to_upper(u8 c);

///////////////////////////
// Strings
///////////////////////////

String string_format(Arena *arena, const char *format, ...);
String string_concat(Arena *arena, u64 count, ...);

b32 string_equal(String s, String t);
b32 string_starts_with(String str, String start);
b32 string_ends_with(String str, String end);

String string_nil();
String string_copy(Arena *arena,String str);

String string_substring(String s, u64 start, u64 len);
s64 string_get_first_index(String s, const char *find, b32 from_end);
b32 string_contains(String s, String find);
String string_replace(Arena *arena, String str, String find, String replacement);

String string_rtrim(String s);
String string_ltrim(String s);
String string_trim(String s);

b32 string_in_list(String str, StringList list);
b32 string_in_array(String str, StringArray arr);

String string_to_lower(Arena *arena, String str);
String string_to_upper(Arena *arena, String str);

u32 string_hash(String s);

// Returns null-terminated C String
char* string_to_cstr(Arena *arena, String str);

void string_print(String s);


///////////////////////////
// String Lists
///////////////////////////

StringList string_list_create(Arena *arena);
void       string_list_add(StringList *sl, String str);
void       string_list_addf(StringList *sl, const char *format, ...);
b32        string_list_remove(StringList *sl, u64 index);
String     string_list_get(StringList *sl, u64 index);
String     string_list_collapse(StringList *sl);

///////////////////////////
// String Arrays
///////////////////////////

StringArray string_array_nil();
StringArray string_array_create(Arena *arena, u64 count, ...);
StringArray string_array_create_empty(Arena *arena, u64 count);
StringArray string_split(Arena *arena, String base, String split);
StringArray string_list_to_array(StringList sl);

///////////////////////////
// Advancement
///////////////////////////

String string_eat_whitespace(String str);
String string_advance(String str, u64 count);
void   string_advance_in_place(String *str, u64 count);
u8     string_get_char_at_index(String str, u64 index);
u8     string_top_char(String str);
u8     string_get_char_and_advance(String *str);
String string_advance_char(String str);

///////////////////////////
// Conversions
///////////////////////////

s64 string_to_s64(String str);
f64 string_to_f64(String str);

///////////////////////////
// C-String Compatibility
///////////////////////////

u64 cstring_strlen(const char *str);

