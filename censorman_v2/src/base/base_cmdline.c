
CmdLine cmdline_parse(Arena *arena, int argc, char *args[])
{
    CmdLine cmdline = {0};
    cmdline.arena = arena;

    if(argc <= 0) return cmdline;

    cmdline.command = STR(args[0]);
    cmdline.args = list_create(arena, sizeof(CmdLineArg));

    for(int i = 1; i < argc; ++i)
    {
        String arg_str = STR(args[i]);
        arg_str = string_trim(arg_str);

        if(arg_str.len == 0) continue;

        CmdLineArg arg = {0};

        if(string_starts_with(arg_str, S("--")))
        {
            arg_str = string_advance(arg_str, 2);
            arg.is_flag = true;
        }
        else if(string_starts_with(arg_str, S("-")))
        {
            arg_str = string_advance(arg_str, 1);
            arg.is_flag = true;
        }

        arg.hash = hash_string(arg_str, 0);

        if(arg.is_flag)
        {
            // check if next argument should be associated to
            // this flag

            if(i < argc - 1)
            {
                String next_arg = STR(args[i+1]);
                if(!string_starts_with(next_arg,S("-")))
                {
                    // assocate next arg as value with this one
                    arg.value = next_arg;
                    i++; // skip next argument
                }
            }
        }
        else
        {
            arg.value = arg_str;
        }

        list_add(&cmdline.args, (void *)&arg);
    }

    return cmdline;
}

b32 cmdline_has_flag(CmdLine *cmdline, String id)
{
    u32 hash = hash_string(id, 0);
    for(u64 i = 0; i < cmdline->args.count; ++i)
    {
        CmdLineArg *arg = (CmdLineArg *)list_get((void*)&cmdline->args, i);
        if(hash == arg->hash && arg->is_flag)
        {
            return true;
        }
    }
    return false;
}

b32 cmdline_has_any_flags(CmdLine *cmdline, StringArray ids)
{
    for(u64 i = 0; i < ids.count; ++i)
    {
        if(cmdline_has_flag(cmdline, ids.items[i]))
            return true;
    }
    return false;
}

String cmdline_get_value(CmdLine *cmdline, String id)
{
    u32 hash = hash_string(id, 0);
    for(u64 i = 0; i < cmdline->args.count; ++i)
    {
        CmdLineArg *arg = (CmdLineArg *)list_get((void*)&cmdline->args, i);
        if(hash == arg->hash && arg->is_flag)
        {
            return arg->value;
        }
    }

    return string_nil();
}

u64 cmdline_get_unflagged_count(CmdLine *cmdline)
{
    u64 count = 0;
    for(u64 i = 0; i < cmdline->args.count; ++i)
    {
        CmdLineArg *arg = (CmdLineArg *)list_get((void*)&cmdline->args, i);
        if(!arg->is_flag)
            count++;
    }
    return count;
}

String cmdline_get_unflagged(CmdLine *cmdline, u64 index)
{
    u64 arg_index = 0;
    for(u64 i = 0; i < cmdline->args.count; ++i)
    {
        CmdLineArg *arg = (CmdLineArg *)list_get((void*)&cmdline->args, i);
        if(!arg->is_flag)
        {
            if(arg_index == index)
                return arg->value;
            arg_index++;
        }
    }

    return string_nil();
}

void cmdline_print(CmdLine *cmdline)
{
    ArenaTemp temp = arena_temp_begin(cmdline->arena);

    StringList sl = string_list_create(temp.arena);
    string_list_addf(&sl, "\nCommand: " STR_FMT, STR_ARG(cmdline->command));

    for(u64 i = 0; i < cmdline->args.count; ++i)
    {
        CmdLineArg *arg = (CmdLineArg *)list_get(&cmdline->args, i);
        string_list_addf(&sl, "\n[Arg %d]: hash: %08X, is_flag: %s, value: " STR_FMT, i, arg->hash, STR_BOOL(arg->is_flag), STR_ARG(arg->value));
    }
    string_list_add(&sl, S("\n"));

    string_print(string_list_collapse(&sl));

    arena_temp_end(temp);
}
