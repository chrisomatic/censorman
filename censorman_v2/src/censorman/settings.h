#pragma once

typedef enum
{
    TYPE_UNSUPPORTED = 0,
    TYPE_IMAGE,
    TYPE_VIDEO,
} AssetType;

typedef struct
{
    AssetType type;
    
} Asset;

typedef struct
{

} Settings;

Settings settings_default();
void settings_parse_cmd_line(Settings *settings, CmdLine cmd_line);
void settings_print(Settings *);
