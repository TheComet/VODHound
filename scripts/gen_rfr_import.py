from pathlib import Path

for path in Path("/home/thecomet/videos/ssbu").rglob("*.rfr"):
    print(f'    if (import_rfr_into_db(dbi, db, "{path}") != 0) break;')

