#!/usr/bin/env python
import csv
import pathlib

root = pathlib.Path(__file__).resolve().parent.parent
translation_dummy_path = root.joinpath("Source/translation_dummy.cpp")

base_paths = {
    "classdat": root.joinpath("assets/txtdata/classes/classdat.tsv"),
    "monstdat": root.joinpath("assets/txtdata/monsters/monstdat.tsv"),
    "unique_monstdat": root.joinpath("assets/txtdata/monsters/unique_monstdat.tsv"),
    "itemdat": root.joinpath("assets/txtdata/items/itemdat.tsv"),
    "unique_itemdat": root.joinpath("assets/txtdata/items/unique_itemdat.tsv"),
    "item_prefixes": root.joinpath("assets/txtdata/items/item_prefixes.tsv"),
    "item_suffixes": root.joinpath("assets/txtdata/items/item_suffixes.tsv"),
    "questdat": root.joinpath("assets/txtdata/quests/questdat.tsv"),
    "spelldat": root.joinpath("assets/txtdata/spells/spelldat.tsv"),
    "textdat": root.joinpath("assets/txtdata/text/textdat.tsv"),
}

hf_paths = {
    "monstdat": root.joinpath("mods/Hellfire/txtdata/monsters/monstdat.tsv"),
    "unique_itemdat": root.joinpath("mods/Hellfire/txtdata/items/unique_itemdat.tsv"),
    "item_prefixes": root.joinpath("mods/Hellfire/txtdata/items/item_prefixes.tsv"),
    "item_suffixes": root.joinpath("mods/Hellfire/txtdata/items/item_suffixes.tsv"),
    "spelldat": root.joinpath("mods/Hellfire/txtdata/spells/spelldat.tsv"),
}

seen_pairs = set()

def write_entry(temp_source, var_name, context, string_value, use_p):
    if not string_value:
        return
    key = (context, string_value)
    if key in seen_pairs:
        return
    seen_pairs.add(key)
    if use_p:
        temp_source.write(f'const char *{var_name} = P_("{context}", "{string_value}");\n')
    else:
        temp_source.write(f'const char *{var_name} = N_("{string_value}");\n')

replacement_table = str.maketrans(
    ' -',
    '__',
    '\''
)

def create_identifier(value, prefix = '', suffix = ''):
    return prefix + value.upper().translate(replacement_table) + suffix

def escape_cpp_string(s):
    """Escape a string for use in a C++ string literal."""
    return s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')

def process_srt_file(srt_path, temp_source, prefix="SUBTITLE"):
    """Parse an SRT file and extract subtitle text for translation."""
    if not srt_path.exists():
        return
    try:
        with open(srt_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception:
        return

    lines = content.split('\n')
    text = ""
    subtitle_index = 0
    i = 0
    while i < len(lines):
        line = lines[i]
        # Remove \r if present (matching C++ parser behavior)
        if line and line.endswith('\r'):
            line = line[:-1]

        # Skip empty lines (end of subtitle block)
        if not line:
            if text:
                # Remove trailing newline from text
                text = text.rstrip('\n')
                if text:
                    var_name = f'{prefix}_{subtitle_index}'
                    escaped_text = escape_cpp_string(text)
                    write_entry(temp_source, var_name, "subtitle", escaped_text, False)
                    subtitle_index += 1
                text = ""
            i += 1
            continue

        # Check if line is a number (subtitle index) - skip it
        if line.strip().isdigit():
            i += 1
            continue

        # Check if line contains --> (timestamp line) - skip it
        if '-->' in line:
            i += 1
            continue

        # Otherwise it's subtitle text
        if text:
            text += "\n"
        text += line
        i += 1

    # Handle last subtitle if file doesn't end with blank line
    if text:
        text = text.rstrip('\n')
        if text:
            var_name = f'{prefix}_{subtitle_index}'
            escaped_text = escape_cpp_string(text)
            write_entry(temp_source, var_name, "subtitle", escaped_text, False)

def process_files(paths, temp_source):
    # Classes
    if "classdat" in paths:
        with open(paths["classdat"], 'r') as tsv:
            reader = csv.DictReader(tsv, delimiter='\t')
            for i, row in enumerate(reader):
                var_name = create_identifier(row['className'], 'CLASS_', '_NAME')
                write_entry(temp_source, var_name, "default", row['className'], False)

    # Monsters
    with open(paths["monstdat"], 'r') as tsv:
        reader = csv.DictReader(tsv, delimiter='\t')
        for row in reader:
            var_name = create_identifier(row['_monster_id'], '', '_NAME')
            write_entry(temp_source, var_name, "monster", row['name'], True)

    if "unique_monstdat" in paths:
        with open(paths["unique_monstdat"], 'r') as tsv:
            reader = csv.DictReader(tsv, delimiter='\t')
            for row in reader:
                var_name = create_identifier(row['name'], '', '_NAME')
                write_entry(temp_source, var_name, "monster", row['name'], True)

    # Items
    if "itemdat" in paths:
        with open(paths["itemdat"], 'r') as tsv:
            reader = csv.DictReader(tsv, delimiter='\t')
            for i, row in enumerate(reader):
                name = row['name']
                if name in ('Scroll of None', 'Non Item', 'Book of '):
                    continue
                shortName = row['shortName']
                if row['id']:
                    base_name = create_identifier(row['id'])
                else:
                    base_name = create_identifier(str(i), 'ITEM_')

                write_entry(temp_source, f'{base_name}_NAME', "default", name, False)
                if shortName:
                    write_entry(temp_source, f'{base_name}_SHORT_NAME', "default", shortName, False)

    with open(paths["unique_itemdat"], 'r') as tsv:
        reader = csv.DictReader(tsv, delimiter='\t')
        for i, row in enumerate(reader):
            write_entry(temp_source, f'UNIQUE_ITEM_{i}_NAME', "default", row['name'], False)

    with open(paths["item_prefixes"], 'r') as tsv:
        reader = csv.DictReader(tsv, delimiter='\t')
        for i, row in enumerate(reader):
            write_entry(temp_source, f'ITEM_PREFIX_{i}_NAME', "default", row['name'], False)

    with open(paths["item_suffixes"], 'r') as tsv:
        reader = csv.DictReader(tsv, delimiter='\t')
        for i, row in enumerate(reader):
            write_entry(temp_source, f'ITEM_SUFFIX_{i}_NAME', "default", row['name'], False)

    # Quests
    if "questdat" in paths:
        with open(paths["questdat"], 'r') as tsv:
            reader = csv.DictReader(tsv, delimiter='\t')
            for i, row in enumerate(reader):
                var_name = create_identifier(row['qlstr'], 'QUEST_', '_NAME')
                write_entry(temp_source, var_name, "default", row['qlstr'], False)

    # Spells
    with open(paths["spelldat"], 'r') as tsv:
        reader = csv.DictReader(tsv, delimiter='\t')
        for i, row in enumerate(reader):
            var_name = create_identifier(row['name'], 'SPELL_', '_NAME')
            write_entry(temp_source, var_name, "spell", row['name'], True)

    # Text/Speeches
    if "textdat" in paths:
        with open(paths["textdat"], 'r') as tsv:
            reader = csv.DictReader(tsv, delimiter='\t')
            for i, row in enumerate(reader):
                write_entry(temp_source, f'TEXT_{i}', "default", row['txtstr'], False)

with open(translation_dummy_path, 'w') as temp_source:
    temp_source.write(f'/**\n')
    temp_source.write(f' * @file translation_dummy.cpp\n')
    temp_source.write(f' *\n')
    temp_source.write(f' * Do not edit this file manually, it is automatically generated\n')
    temp_source.write(f' * and updated by the extract_translation_data.py script.\n')
    temp_source.write(f' */\n')
    temp_source.write(f'#include "utils/language.h"\n\n')
    temp_source.write(f'namespace {{\n\n')

    process_files(base_paths, temp_source)
    process_files(hf_paths, temp_source)

    # Process SRT subtitle files
    srt_files = [
        root.joinpath("assets/gendata/diabend.srt"),
    ]
    for srt_file in srt_files:
        # Extract filename without extension and convert to uppercase for prefix
        filename = srt_file.stem.upper()
        prefix = f"SUBTITLE_{filename}"
        process_srt_file(srt_file, temp_source, prefix)

    temp_source.write(f'\n}} // namespace\n')
