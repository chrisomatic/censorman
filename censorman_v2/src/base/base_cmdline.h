#pragma once

typedef struct
{
    u32 hash;
    b32 is_flag;
    String value;
} CmdLineArg;

typedef struct
{
    Arena* arena;
    String command;
    List   args;
} CmdLine;

CmdLine cmdline_parse(Arena *arena, char *args[], int argc);
void    cmdline_print(CmdLine *cmdline);

// flagged arguments (-) or (--)
b32    cmdline_has_flag(CmdLine *cmdline,  String id);
b32    cmdline_has_any_flags(CmdLine *cmdline, StringArray ids);
String cmdline_get_value(CmdLine *cmdline, String id);

// unflagged arguments
u64    cmdline_get_unflagged_count(CmdLine *cmdline);
String cmdline_get_unflagged(CmdLine *cmdline, u64 index);
