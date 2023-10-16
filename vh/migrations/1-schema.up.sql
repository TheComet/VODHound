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
    UNIQUE (file_name)
);
CREATE TABLE IF NOT EXISTS video_paths (
    path TEXT NOT NULL,
    UNIQUE (path)
);
CREATE TABLE IF NOT EXISTS sponsors (
    id INTEGER PRIMARY KEY NOT NULL,
    short_name TEXT NOT NULL,
    full_name TEXT NOT NULL,
    website TEXT NOT NULL,
    CHECK (short_name <> '' OR full_name <> ''),
    UNIQUE (short_name, full_name, website)
);
CREATE TABLE IF NOT EXISTS people (
    id INTEGER PRIMARY KEY NOT NULL,
    sponsor_id INTEGER,
    name TEXT NOT NULL,
    tag TEXT NOT NULL,
    social TEXT NOT NULL,
    pronouns TEXT NOT NULL,
    FOREIGN KEY (sponsor_id) REFERENCES sponsors(id),
    UNIQUE (name)
);
CREATE TABLE IF NOT EXISTS tournaments (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    website TEXT NOT NULL,
    UNIQUE (name, website),
    CHECK (name <> '')
);
CREATE TABLE IF NOT EXISTS tournament_organizers (
    tournament_id INTEGER NOT NULL CHECK (tournament_id > 0),
    person_id INTEGER NOT NULL CHECK (person_id > 0),
    UNIQUE (tournament_id, person_id)
    FOREIGN KEY (tournament_id) REFERENCES tournaments(id),
    FOREIGN KEY (person_id) REFERENCES people(id)
);
CREATE TABLE IF NOT EXISTS tournament_sponsors (
    tournament_id INTEGER NOT NULL CHECK (tournament_id > 0),
    sponsor_id INTEGER NOT NULL CHECK (sponsor_id > 0),
    UNIQUE (tournament_id, sponsor_id),
    FOREIGN KEY (tournament_id) REFERENCES tournaments(id),
    FOREIGN KEY (sponsor_id) REFERENCES sponsors(id)
);
CREATE TABLE IF NOT EXISTS tournament_commentators (
    tournament_id INTEGER NOT NULL CHECK (tournament_id > 0),
    person_id INTEGER NOT NULL CHECK (person_id > 0),
    UNIQUE (tournament_id, person_id),
    FOREIGN KEY (tournament_id) REFERENCES tournaments(id),
    FOREIGN KEY (person_id) REFERENCES people(id)
);
CREATE TABLE IF NOT EXISTS set_formats (
    id INTEGER PRIMARY KEY NOT NULL,
    short_name TEXT NOT NULL,
    long_name TEXT NOT NULL,
    UNIQUE (short_name),
    UNIQUE (long_name)
);
CREATE TABLE IF NOT EXISTS event_types (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    UNIQUE (name)
);
CREATE TABLE IF NOT EXISTS events (
    id INTEGER PRIMARY KEY NOT NULL,
    event_type_id INTEGER NOT NULL,
    url TEXT NOT NULL,
    UNIQUE (event_type_id, url),
    FOREIGN KEY (event_type_id) REFERENCES event_types(id)
);
CREATE TABLE IF NOT EXISTS round_types (
    id INTEGER PRIMARY KEY NOT NULL,
    short_name TEXT NOT NULL,
    long_name TEXT NOT NULL,
    UNIQUE (short_name),
    UNIQUE (long_name)
);
CREATE TABLE IF NOT EXISTS teams (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    url TEXT NOT NULL,
    UNIQUE (name, url)
);
CREATE TABLE IF NOT EXISTS team_members (
    team_id INTEGER NOT NULL,
    person_id INTEGER NOT NULL,
    UNIQUE (team_id, person_id),
    FOREIGN KEY (team_id) REFERENCES teams(id),
    FOREIGN KEY (person_id) REFERENCES people(id)
);
CREATE TABLE IF NOT EXISTS games (
    id INTEGER PRIMARY KEY NOT NULL,
    -- Round type only exists for brackets.
    -- For Money match, Practice, Amateurs etc. this will be NULL.
    round_type_id INTEGER,
    -- Round number is NULL when the round type is semi-finals, finals, or any
    -- other type that doesn't need a number.
    round_number INTEGER,
    set_format_id INTEGER NOT NULL,
    winner_team_id INTEGER NOT NULL,
    stage_id INTEGER NOT NULL,
    time_started TIMESTAMP NOT NULL,
    time_ended TIMESTAMP NOT NULL,
    FOREIGN KEY (round_type_id) REFERENCES round_types(id),
    FOREIGN KEY (set_format_id) REFERENCES set_formats(id),
    FOREIGN KEY (winner_team_id) REFERENCES teams(id),
    FOREIGN KEY (stage_id) REFERENCES stages(id),
    UNIQUE (time_started),
    CHECK (round_type_id > 0),
    CHECK (round_number > 0),
    CHECK (set_format_id > 0),
    CHECK (winner_team_id > 0),
    CHECK (stage_id > -1)  -- Stage 0 is valid
);
CREATE TABLE IF NOT EXISTS tournament_games (
    game_id INTEGER NOT NULL,
    tournament_id INTEGER NOT NULL,
    FOREIGN KEY (game_id) REFERENCES games(id),
    FOREIGN KEY (tournament_id) REFERENCES tournaments(id),
    UNIQUE (game_id)
);
CREATE TABLE IF NOT EXISTS event_games (
    game_id INTEGER NOT NULL,
    event_id INTEGER NOT NULL,
    FOREIGN KEY (game_id) REFERENCES games(id),
    FOREIGN KEY (event_id) REFERENCES events(id),
    UNIQUE (game_id)
);
CREATE TABLE IF NOT EXISTS game_players (
    person_id INTEGER NOT NULL,
    game_id INTEGER NOT NULL,
    slot INTEGER NOT NULL,
    team_id INTEGER NOT NULL,
    fighter_id INTEGER NOT NULL,
    costume INTEGER NOT NULL,
    is_loser_side BOOLEAN NOT NULL CHECK (is_loser_side IN (0, 1)),
    UNIQUE (person_id, game_id),
    FOREIGN KEY (person_id) REFERENCES people(id),
    FOREIGN KEY (game_id) REFERENCES games(id),
    FOREIGN KEY (team_id) REFERENCES teams(id),
    FOREIGN KEY (fighter_id) REFERENCES fighters(id)
);
CREATE TABLE IF NOT EXISTS game_videos (
    game_id INTEGER NOT NULL,
    video_id INTEGER NOT NULL,
    frame_offset INTEGER NOT NULL,
    FOREIGN KEY (game_id) REFERENCES games(id),
    FOREIGN KEY (video_id) REFERENCES videos(id),
    UNIQUE (game_id, video_id)
);
CREATE TABLE IF NOT EXISTS scores (
    game_id INTEGER NOT NULL,
    team_id INTEGER NOT NULL,
    score INTEGER NOT NULL CHECK (score >= 0),
    UNIQUE (game_id, team_id, score),
    FOREIGN KEY (game_id) REFERENCES games(id),
    FOREIGN KEY (team_id) REFERENCES teams(id)
);
CREATE TABLE IF NOT EXISTS frames (
    id INTEGER PRIMARY KEY NOT NULL,
    game_id INTEGER NOT NULL CHECK (game_id > 0),
    slot INTEGER NOT NULL,
    time_stamp TIMESTAMP NOT NULL,
    frame_number INTEGER NOT NULL,
    frames_left INTEGER NOT NULL,
    posx FLOAT NOT NULL,
    posy FLOAT NOT NULL,
    damage FLOAT NOT NULL,
    hitstun FLOAT NOT NULL,
    shield FLOAT NOT NULL,
    status_id INTEGER NOT NULL,
    hit_status_id INTEGER NOT NULL,
    hash40 INTEGER NOT NULL,
    stocks INTEGER NOT NULL,
    attack_connected BOOLEAN NOT NULL CHECK (attack_connected IN (0, 1)),
    facing_left BOOLEAN NOT NULL CHECK (facing_left IN (0, 1)),
    opponent_in_hitlag BOOLEAN NOT NULL CHECK (opponent_in_hitlag IN (0, 1)),
    FOREIGN KEY (game_id) REFERENCES games(id),
    FOREIGN KEY (status_id) REFERENCES status_enums(id),
    FOREIGN KEY (hash40) REFERENCES motions(hash40),
    FOREIGN KEY (hit_status_id) REFERENCES hit_status_enums(id)
);

