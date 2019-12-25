# mkvtaginator

The express purpose of this program is to tag Matroska (MKV) files with metadata, and optionally embed cover art into them. Uses `mkvmerge` and friends behind the scenes (usually part of the `mkvtoolnix` package).

For the list of command-line options, use `--help`.


### Short feature list

1. Automatic filename parsing (of the format `SERIES_TITLE S04E14 - EPISODE_TITLE.mkv`, episode title is optional)
2. Metadata search; user-selection for ambiguous results (select-once-and-remember)
3. Cover art embedding; automatic if standard image names detected in current folder
4. Option for in-place editing, or copying to a new file


### Usage

Basic usage is: `./mkvtaginator [options] <input files>`. You can pass more than one file to the program, but they must all have the `.mkv`
extension (and obviously must be valid mkv files).

It's recommended to use `--dry-run` to preview the command-line for `mkvpropedit`, especially when editing in-place (the default). Speaking
of which, `mkvpropedit`, `mkvextract`, and `mkvmerge` must be in `$PATH` to work. There are no other external dependencies.

Since in the typical use-case you probably already know the correct series, use `--id <API_SPECIFIC_ID>` to manually specify the identifer
of the series/movie, to skip searching and ensure that no user interaction is required.

To enable "out-of-place" output (ie. the input files are copied, and the new copy is modified with the originals untouched), simply use
`--output-folder <FOLDER_PATH>`; for any given input file, the output path must not coincide with the path of the input &mdash; since that
would just overwrite it.


### Cover art detection

If a file with the following names exists in the current folder, then it is automatically chosen for embedding (disable this with `--no-auto-cover`):

1. `cover`
2. `season1`
3. `season01`
4. `Season1`
5. `Season01`
6. `season`
7. `Season`
8. `poster`

The preferred file is first (ie. it will prefer a file `cover.jpg` over `poster.jpg` if both exist), and `png` files are preferred over
`jpg` and `jpeg`.


### Metadata sources

Currently, the only supported metadata source is [theTVDB](https://thetvdb.com). You must create your own API key, and pass it to
the program using `--tvdb-api <API_KEY>`. More will come.


### Caveats

For some reason, certain programs (including Windows with the Icaros extension, MetaX, and probably others) only detect "valid" cover art
if it's the first attachment in the file. For most files this is not an issue, but for anime with meticulous typesetting, there will
often be lots of font attachments. Adding the cover art at the end, after the fonts, has been known to prevent programs from seeing the
cover art.

Thus, this program will always ensure that the cover art attachment is the first attachment in the file. If there are no other attachments,
it is straightforward. If not, then the first attachment is extracted to a temporary file, replaced with the cover art, and the temporary
file is attached back to the end of the file as the last attachment.

Note that `mkvtaginator` is not indempotent in terms of cover art; running it multiple times on the same file (with in-place editing) will
cause lots of cover-art attachments to pile up in the file.


### Contributing

Feel free to open an issue if you feel that there's any missing feature, or if you found a bug. Pull requests are also welcome.



### Licensing

The project itself (this repository) is licensed under the Apache 2.0 license (see `LICENSE` file). For ease of building, some dependencies
are included in the repository itself and compiled together, instead of as a separate library (shared or otherwise).

These are:

1. [tinyxml2](https://github.com/leethomason/tinyxml2), zlib
2. [picojson](https://github.com/kazuho/picojson), BSD
3. [wcwidth](https://github.com/termux/wcwidth), MIT
4. [c++requests](https://github.com/whoshuu/cpr), MIT
5. [tinyprocesslib](https://gitlab.com/eidheim/tiny-process-library), MIT



### API attribution

TV information is provided by TheTVDB.com, but we are not endorsed or certified by TheTVDB.com or its affiliates.

This product uses the TMDb API but is not endorsed or certified by TMDb.









