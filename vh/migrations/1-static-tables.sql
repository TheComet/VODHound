INSERT INTO event_types (id, name) VALUES
    (0, 'Singles Bracket'),
    (1, 'Doubles Bracket'),
    (2, 'Side Bracket'),
    (3, 'Amateurs Bracket'),
    (4, 'Money Match'),
    (5, 'Practice'),
    (6, 'Friendlies');
INSERT INTO round_types (id, short_name, long_name) VALUES
    (0,  'WR',    'Winners Round'),
    (1,  'WQF',   'Winners Quarter Finals'),
    (2,  'WSF',   'Winners Semi Finals'),
    (3,  'WF',    'Winners Finals'),
    (4,  'LR',    'Losers Round'),
    (5,  'LQF',   'Losers Quarter Finals'),
    (6,  'LSF',   'Losers Semi Finals'),
    (7,  'LF',    'Losers Finals'),
    (8,  'GF',    'Grand Finals'),
    (9,  'Pools', 'Pools'),
    (10, 'GFR',   'Grand Finals Reset');
INSERT INTO set_formats (id, short_name, long_name) VALUES
    (0, 'Bo3',  'Best of 3'),
    (1, 'Bo5',  'Best of 5'),
    (2, 'Bo7',  'Best of 7'),
    (3, 'FT5',  'First to 5'),
    (4, 'FT10', 'First to 10'),
    (5, 'Free', 'Free Play');

