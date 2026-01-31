Build:
- Run `make` from the project root. Produces `defrag`.

Usage:
Method1:
- `./defrag <input_image_path>` will create disk_defrag file as output
- `diff disk_defrag <expected_image_path>` will compare both the files

Method2:
- Verify against expected: `./defrag <input_image> --verify <expected_image>`
  Example: `./defrag images_frag/disk_frag_1 --verify images_defrag/disk_defrag_1`

