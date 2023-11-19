#include "application/fighter_icons.h"
#include "vh/mem.h"
#include "vh/str.h"

static const char* files[94] = {
    "mario",
    "donkey",
    "link",
    "samus",
    "samusd",
    "yoshi",
    "kirby",
    "fox",
    "pikachu",
    "luigi",
    "ness",
    "captain",
    "purin",
    "peach",
    "daisy",
    "koopa",
    "sheik",
    "zelda",
    "mariod",
    "pichu",
    "falco",
    "marth",
    "lucina",
    "younglink",
    "ganon",
    "mewtwo",
    "roy",
    "chrom",
    "gamewatch",
    "metaknight",
    "pit",
    "pitb",
    "szerosuit",
    "wario",
    "snake",
    "ike",
    "pzenigame",
    "pfushigisou",
    "plizardon",
    "diddy",
    "lucas",
    "sonic",
    "dedede",
    "pikmin",
    "lucario",
    "robot",
    "toonlink",
    "wolf",
    "murabito",
    "rockman",
    "wiifit",
    "rosetta",
    "littlemac",
    "gekkouga",
    "palutena",
    "pacman",
    "reflet",
    "shulk",
    "koopajr",
    "duckhunt",
    "ryu",
    "ken",
    "cloud",
    "kamui",
    "bayonetta",
    "inkling",
    "ridley",
    "simon",
    "richter",
    "krool",
    "shizue",
    "gaogaen",
    "miifighter",
    "miiswordsman",
    "miigunner",
    "ice_climber",
    "ice_climber",
    "",
    "miifighter",
    "miiswordsman",
    "miigunner",
    "packun",
    "jack",
    "brave",
    "buddy",
    "dolly",
    "master",
    "tantan",
    "pickel",
    "edge",
    "eflame",
    "elight",
    "demon",
    "trail",
};

static char*
build_str(const char* path, const char* fighter, char costume)
{
    struct str str;
    struct str_view costume_v = { &costume, 1 };
    str_init(&str);
    cstr_set(&str, path);
    cstr_append(&str, "chara_2_");
    cstr_append(&str, fighter);
    cstr_append(&str, "_0");
    str_append(&str, costume_v);
    cstr_append(&str, ".png");
    str_terminate(&str);
    return str.data;
}

static char* default_str = "";

char*
fighter_icon_get_resource_path_from_id(int fighter_id, int costume)
{
    if (fighter_id < 0 || fighter_id >= 94)
        return default_str;
    if (costume < 0 || costume > 7)
        costume = 0;
    return build_str(
        "/ch/thecomet/vodhound/ssbu_icons/",
        files[fighter_id],
        "01234567"[costume]);
}

void
fighter_icon_free_str(char* str)
{
    if (str != default_str)
        mem_free(str);
}
