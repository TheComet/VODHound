CREATE TABLE stages (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);
CREATE TABLE fighters (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);
CREATE TABLE hit_status_enums (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);
CREATE TABLE status_enums (
    id INTEGER PRIMARY KEY NOT NULL,
    fighter_id INTEGER,
    value INTEGER NOT NULL,
    name TEXT NOT NULL,
    FOREIGN KEY (fighter_id) REFERENCES fighters(id)
);
CREATE TABLE motions (
    hash40 INTEGER PRIMARY KEY NOT NULL,
    string TEXT NOT NULL
);
CREATE TABLE motion_categories (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);
CREATE TABLE motion_labels (
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
CREATE TABLE motion_usages (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);
CREATE TABLE motion_layers (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);
CREATE TABLE videos (
    id INTEGER PRIMARY KEY NOT NULL,
    file_name TEXT NOT NULL,
    frame_offset INTEGER NOT NULL
);
CREATE TABLE video_paths (
    path TEXT PRIMARY KEY NOT NULL
);
CREATE TABLE people (
    id INTEGER PRIMARY KEY NOT NULL,
    sponsor TEXT,
    name TEXT NOT NULL,
    tag TEXT NOT NULL,
    social TEXT,
    pronouns TEXT
);
CREATE TABLE tournaments (
    id INTEGER PRIMAY KEY,
    name TEXT NOT NULL,
    website TEXT
);
CREATE TABLE tournament_organizers (
    tournament_id INTEGER NOT NULL,
    person_id INTEGER NOT NULL,
    FOREIGN KEY (tournament_id) REFERENCES tournaments(id),
    FOREIGN KEY (person_id) REFERENCES people(id)
);
CREATE TABLE sponsors (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL, website TEXT
);
CREATE TABLE tournament_sponsors (
    tournament_id INTEGER NOT NULL,
    sponsor_id INTEGER NOT NULL,
    FOREIGN KEY (tournament_id) REFERENCES tournaments(id),
    FOREIGN KEY (sponsor_id) REFERENCES sponsors(id)
);
CREATE TABLE tournament_commentators (
    tournament_id INTEGER NOT NULL,
    person_id INTEGER NOT NULL,
    FOREIGN KEY (tournament_id) REFERENCES tournaments(id),
    FOREIGN KEY (person_id) REFERENCES people(id)
);
CREATE TABLE bracket_types (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);
CREATE TABLE round_types (
    id INTEGER PRIMARY KEY NOT NULL,
    short_name TEXT NOT NULL,
    long_name TEXT NOT NULL
);
CREATE TABLE set_formats (
    id INTEGER PRIMARY KEY NOT NULL,
    short_name TEXT NOT NULL,
    long_name TEXT NOT NULL
);
CREATE TABLE brackets (
    id INTEGER PRIMARY KEY NOT NULL,
    bracket_type_id INTEGER NOT NULL,
    url TEXT,
    FOREIGN KEY (bracket_type_id) REFERENCES bracket_types(id)
);
CREATE TABLE rounds (
    id INTEGER PRIMARY KEY NOT NULL,
    round_type_id,
    number INTEGER NOT NULL,
    FOREIGN KEY (round_type_id) REFERENCES rount_types(id)
);
CREATE TABLE teams (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);
CREATE TABLE games (
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
CREATE TABLE scores (
    game_id INTEGER NOT NULL,
    person_id INTEGER NOT NULL,
    score INTEGER NOT NULL,
    FOREIGN KEY (game_id) REFERENCES games(id),
    FOREIGN KEY (person_id) REFERENCES people(id)
);
CREATE TABLE player_games (
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
CREATE TABLE frame_data (
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

