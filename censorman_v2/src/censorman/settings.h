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

void settings_parse_cmd_line(Settings *settings, int argc, char *args);
void settings_print(Settings *settings);
