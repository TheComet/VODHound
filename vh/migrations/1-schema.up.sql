CREATE TABLE IF NOT EXISTS stages (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS fighters (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS hit_status_enums (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS status_enums (
    id INTEGER PRIMARY KEY NOT NULL,
    fighter_id INTEGER,
    value INTEGER NOT NULL,
    name TEXT NOT NULL,
    FOREIGN KEY (fighter_id) REFERENCES fighters(id)
);
CREATE TABLE IF NOT EXISTS motions (
    hash40 INTEGER PRIMARY KEY NOT NULL,
    string TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS motion_categories (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS motion_labels (
    id INTEGER PRIMARY KEY NOT NULL,
    hash40 INTEGER NOT NULL,
    layer_id INTEGER NOT NULL,
    category_id INTEGER NOT NULL,
    usage_id INTEGER NOT NULL,
    label TEXT NOT NULL,
    FOREIGN KEY (hash40) REFERENCES motions(hash40),
    FOREIGN KEY (layer_id) REFERENCES motion_layers(id),
    FOREIGN KEY (category_id) REFERENCES motion_categories(id),
    FOREIGN KEY (usage_id) REFERENCES motion_usages(id)
);
CREATE TABLE IF NOT EXISTS motion_usages (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS motion_layers (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS videos (
    id INTEGER PRIMARY KEY NOT NULL,
    file_name TEXT NOT NULL,
    frame_offset INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS video_paths (
    path TEXT PRIMARY KEY NOT NULL
);
CREATE TABLE IF NOT EXISTS people (
    id INTEGER PRIMARY KEY NOT NULL,
    sponsor_id INTEGER,
    name TEXT NOT NULL,
    tag TEXT NOT NULL,
    social TEXT NOT NULL,
    pronouns TEXT NOT NULL,
    FOREIGN KEY (sponsor_id) REFERENCES sponsors(id),
    UNIQUE(name)
);
CREATE TABLE IF NOT EXISTS tournaments (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    website TEXT NOT NULL,
    UNIQUE(name, website)
);
CREATE TABLE IF NOT EXISTS tournament_organizers (
    tournament_id INTEGER NOT NULL CHECK (tournament_id > 0),
    person_id INTEGER NOT NULL CHECK (person_id > 0),
    UNIQUE(tournament_id, person_id)
    FOREIGN KEY (tournament_id) REFERENCES tournaments(id),
    FOREIGN KEY (person_id) REFERENCES people(id)
);
CREATE TABLE IF NOT EXISTS sponsors (
    id INTEGER PRIMARY KEY NOT NULL,
    short_name TEXT NOT NULL,
    full_name TEXT NOT NULL,
    website TEXT NOT NULL,
    UNIQUE(short_name, full_name, website)
);
CREATE TABLE IF NOT EXISTS tournament_sponsors (
    tournament_id INTEGER NOT NULL CHECK (tournament_id > 0),
    sponsor_id INTEGER NOT NULL CHECK (sponsor_id > 0),
    UNIQUE(tournament_id, sponsor_id),
    FOREIGN KEY (tournament_id) REFERENCES tournaments(id),
    FOREIGN KEY (sponsor_id) REFERENCES sponsors(id)
);
CREATE TABLE IF NOT EXISTS tournament_commentators (
    tournament_id INTEGER NOT NULL CHECK (tournament_id > 0),
    person_id INTEGER NOT NULL CHECK (person_id > 0),
    UNIQUE(tournament_id, person_id),
    FOREIGN KEY (tournament_id) REFERENCES tournaments(id),
    FOREIGN KEY (person_id) REFERENCES people(id)
);
CREATE TABLE IF NOT EXISTS bracket_types (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    UNIQUE (name)
);
CREATE TABLE IF NOT EXISTS round_types (
    id INTEGER PRIMARY KEY NOT NULL,
    short_name TEXT NOT NULL,
    long_name TEXT NOT NULL,
    UNIQUE (short_name),
    UNIQUE (long_name)
);
CREATE TABLE IF NOT EXISTS set_formats (
    id INTEGER PRIMARY KEY NOT NULL,
    short_name TEXT NOT NULL,
    long_name TEXT NOT NULL,
    UNIQUE (short_name),
    UNIQUE (long_name)
);
CREATE TABLE IF NOT EXISTS brackets (
    id INTEGER PRIMARY KEY NOT NULL,
    bracket_type_id INTEGER NOT NULL,
    url TEXT NOT NULL,
    UNIQUE (url),
    FOREIGN KEY (bracket_type_id) REFERENCES bracket_types(id)
);
CREATE TABLE IF NOT EXISTS rounds (
    id INTEGER PRIMARY KEY NOT NULL,
    round_type_id INTEGER NOT NULL,
    number INTEGER NOT NULL,
    UNIQUE (round_type_id, number),
    FOREIGN KEY (round_type_id) REFERENCES rount_types(id)
);
CREATE TABLE IF NOT EXISTS teams (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS games (
    id INTEGER PRIMARY KEY NOT NULL,
    video_id INTEGER,
    tournament_id INTEGER,
    bracket_id INTEGER,
    round_id INTEGER NOT NULL,
    set_format_id INTEGER,
    winner_team_id INTEGER NOT NULL,
    stage_id INTEGER NOT NULL,
    time_started TIMESTAMP NOT NULL,
    time_ended TIMESTAMP,
    FOREIGN KEY (video_id) REFERENCES videos(id),
    FOREIGN KEY (tournament_id) REFERENCES tournaments(id),
    FOREIGN KEY (bracket_id) REFERENCES brackets(id),
    FOREIGN KEY (round_id) REFERENCES rounds(id),
    FOREIGN KEY (set_format_id) REFERENCES set_formats(id),
    FOREIGN KEY (winner_team_id) REFERENCES teams(id),
    FOREIGN KEY (stage_id) REFERENCES stages(id)
);
CREATE TABLE IF NOT EXISTS scores (
    game_id INTEGER NOT NULL,
    person_id INTEGER NOT NULL,
    score INTEGER NOT NULL,
    FOREIGN KEY (game_id) REFERENCES games(id),
    FOREIGN KEY (person_id) REFERENCES people(id)
);
CREATE TABLE IF NOT EXISTS player_games (
    person_id INTEGER NOT NULL,
    game_id INTEGER NOT NULL,
    slot INTEGER NOT NULL,
    team_id INTEGER NOT NULL,
    fighter_id INTEGER NOT NULL,
    costume INTEGER NOT NULL,
    is_loser_side BOOLEAN NOT NULL,
    FOREIGN KEY (person_id) REFERENCES people(id),
    FOREIGN KEY (game_id) REFERENCES games(id),
    FOREIGN KEY (team_id) REFERENCES teams(id),
    FOREIGN KEY (fighter_id) REFERENCES fighters(id)
);
CREATE TABLE IF NOT EXISTS frame_data (
    game_id INTEGER NOT NULL,
    time_stamp TIMESTAMP NOT NULL,
    frame_number INTEGER NOT NULL,
    frames_left INTEGER NOT NULL,
    posx FLOAT NOT NULL,
    posy FLOAT NOT NULL,
    damage FLOAT NOT NULL,
    hitstun FLOAT NOT NULL,
    shield FLOAT NOT NULL,
    status_id INTEGER NOT NULL,
    hash40_id INTEGER NOT NULL,
    hit_status_id INTEGER NOT NULL,
    stocks INTEGER NOT NULL,
    attack_connected BOOLEAN NOT NULL,
    facing_left BOOLEAN NOT NULL,
    opponent_in_hitlag BOOLEAN NOT NULL,
    FOREIGN KEY (game_id) REFERENCES games(id),
    FOREIGN KEY (status_id) REFERENCES status_enums(id),
    FOREIGN KEY (hash40_id) REFERENCES motions(hash40),
    FOREIGN KEY (hit_status_id) REFERENCES hit_status_enums(id)
);

