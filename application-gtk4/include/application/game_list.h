#pragma once

typedef struct _GtkWidget GtkWidget;
struct db_interface;
struct db;

GtkWidget*
game_list_new(struct db_interface* dbi, struct db* db);
