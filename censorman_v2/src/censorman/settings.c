
Settings settings_default()
{

}

Settings settings_parse_cmd_line(int argc, char *args)
{
    Temp scratch = scratch_begin();

    CmdLine cmdline = cmdline_parse(scratch.arena, argc, args);


    scratch_end(scratch);
}

void settings_print(Settings *settings)
{

}
