#!/usr/bin/env python3
"""Compile and exercise the pinned official Payload Manager C JSON parser."""
import json
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = "e4e72dc9d831dd09ee8fabc222ad1e7f5139050e"
BASE = f"https://raw.githubusercontent.com/itsPLK/ps5-payload-manager/{UPSTREAM}/"


def main():
    catalog_path = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / "payloads.json"
    document = json.loads(catalog_path.read_text())
    expected = document["payloads"][0]
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        for name in ["src/repository.c", "src/json_helpers.c", "src/utils.c", "include/repository.h", "include/json_helpers.h"]:
            with urllib.request.urlopen(BASE + name, timeout=60) as response:
                (work / Path(name).name).write_bytes(response.read())
        # Compile the exact unmodified upstream function with its real JSON
        # helpers. Other repository functions depend on PS5 installation APIs.
        source = (work / "repository.c").read_text()
        start = source.index("int parse_repository_payloads(")
        end = source.index("static int load_cached_repository", start)
        (work / "parser.c").write_text('#include <stdlib.h>\n#include <string.h>\n#include "repository.h"\n#include "json_helpers.h"\n' + source[start:end])
        utilities = (work / "utils.c").read_text()
        start = utilities.index("void pldmgr_utils_get_payload_folder_name(")
        end = utilities.index("void pldmgr_json_escape(", start)
        (work / "folder.c").write_text('#include <string.h>\n#include <stddef.h>\n' + utilities[start:end])
        (work / "main.c").write_text(r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "repository.h"
#include "json_helpers.h"
void pldmgr_utils_get_payload_folder_name(const char *, char *, size_t);
int main(int argc, char **argv) {
    char *text = NULL; size_t length = 0, count = 0; RepoPayload *items = NULL;
    if (argc != 7 || read_file_text(argv[1], &text, &length)) return 1;
    if (parse_repository_payloads(text, &items, &count) || count != 1) return 2;
    int bad = strcmp(items[0].name, "331HEN") || strcmp(items[0].filename, argv[6])
        || strcmp(items[0].url, argv[2]) || strcmp(items[0].checksum, argv[3])
        || strcmp(items[0].version, argv[4]) || strcmp(items[0].category, argv[5]);
    free(items); free(text);
    items = NULL; count = 0;
    if (parse_repository_payloads("[]", &items, &count) || count != 0) return 3;
    free(items); items = NULL; count = 0;
    if (parse_repository_payloads("[{\"name\":\"bad\",\"filename\":\"bad.elf\"}]", &items, &count)
        || count != 0) return 4;
    free(items);
    char folder[128];
    pldmgr_utils_get_payload_folder_name(argv[6], folder, sizeof(folder));
    if (strcmp(folder, "331HEN")) return 6;
    pldmgr_utils_get_payload_folder_name("331HEN.elf", folder, sizeof(folder));
    if (strcmp(folder, "331HEN")) return 7;
    return bad ? 5 : 0;
}
''')
        binary = work / "parser-test"
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-g",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-I", str(work), str(work / "parser.c"), str(work / "json_helpers.c"),
                        str(work / "folder.c"), str(work / "main.c"), "-o", str(binary)], check=True)
        subprocess.run([str(binary), str(catalog_path), expected["url"],
                        expected["checksum"], expected["version"], expected["category"], expected["filename"]], check=True)
        # Verify the mirror's array layout works as well as the custom wrapper.
        array = work / "array.json"
        array.write_text(json.dumps(document["payloads"]))
        subprocess.run([str(binary), str(array), expected["url"], expected["checksum"],
                        expected["version"], expected["category"], expected["filename"]], check=True)
    print(f"Official C parser/folder logic {UPSTREAM}: custom source, mirror-style array, field values, negative cases and versioned/legacy folder mapping passed (ASan/UBSan)")


if __name__ == "__main__":
    main()
