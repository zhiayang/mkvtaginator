# mkvtaginator

The express purpose of this program is to tag Matroska (MKV) files with metadata, and optionally embed cover art into them. Uses
`mkvmerge` and friends behind the scenes (usually part of the `mkvtoolnix` package). It can also mux your "source" files using
`ffmpeg` (`libavformat`) (which you must install).

For the list of command-line options, use `--help`.

As for dependencies, `mkvpropedit`, `mkvextract`, and `mkvmerge` must be in `$PATH` to work. `libcurl` and `ffmpeg` (specifically `libavformat`) are library dependencies.


### Short feature list

1. Automatic filename parsing for tv and movies
2. Metadata search; user-selection for ambiguous results (select-once-and-remember)
3. Cover art embedding; automatic if standard image names detected in current folder
4. Option for in-place editing, or copying to a new file
5. Automatic muxing with smart stream selection, and configurable language preferences (eg. `jpn` for audio, `eng` for subs)


### Usage

Basic usage is: `./mkvtaginator (--tag|--mux) [options] <input files>`. You can pass more than one file to the program,
but they must all have the `.mkv` extension (and obviously must be valid mkv files). Use `--tag` to enable metadata tagging,
`--mux` to enable input muxing, or both to do... both.

It's recommended to use `--dry-run` first to preview the command-line for `mkvpropedit`, especially when editing in-place (the default).

Since in the typical use-case you probably already know the correct series, use `--id <API_SPECIFIC_ID>` to manually specify the identifer
of the series/movie, to skip searching and ensure that no user interaction is required.

To enable "out-of-place" output (ie. the input files are copied, and the new copy is modified with the originals untouched), simply use
`--output-folder <FOLDER_PATH>`; for any given input file, the output path must not coincide with the path of the input &mdash; since that
would just overwrite it.

To rename the output file into so-called "canonical" form, use `--rename`. This will rename TV shows into the form
`SERIES_NAME S01E01 - EPISODE_TITLE.mkv` (the episode title is omitted if none exists), and movies into the form `TITLE (YEAR).mkv`. The
extension will always be `mkv`.



### Muxing

`mkvtaginator` can now "mux" your source files directly; this involves selecting streams matching certain criteria, and copying them
to an output file using `libavformat`. There are 6 options involving this, which can be found in the `--help` menu. Basically it lets
you control what kind of subtitles you prefer (SDH, text formats, signs/songs only).

More interesting is the language selection; you can specify the priority of languages for audio and subtitles independently. This can
be specified either on the command line or using the config file.



### Configuration

All options can be set from the command line; some things don't make sense to be set from the configuration file. The default path
is `$HOME/.config/mkvtaginator/config.json`, failing which it looks for `mkvtaginator-config.json` and `.mkvtaginator-config.json` in
the current directory. If none of those are present, it uses default options.

The default options, as well as the sample configuration file (and its schema) can be found in `sample-config.json` in this repository.



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

Currently, `mkvtaginator` uses [TheTVDB](https://thetvdb.com) and [TheMovieDB](https://themoviedb.org) as metadata sources. You must
create your own API key, and pass it to the program using `--tvdb-api <API_KEY>` and `--moviedb-api <API_KEY>`.


### Caveats

For some reason, certain programs (including Windows with the Icaros extension, MetaX, and probably others) only detect "valid" cover art
if it's the first attachment in the file. For most files this is not an issue, but for anime with meticulous typesetting, there will
often be lots of font attachments. Adding the cover art at the end, after the fonts, has been known to prevent programs from seeing the
cover art.

Thus, `mkvtaginator` will always ensure that the cover art attachment is the first attachment in the file. If there are no other
attachments, it is straightforward. If not, then the first attachment is extracted to a temporary file, replaced with the cover art,
and the temporary file is attached back to the end of the file as the last attachment.

If the first attachment (and only the first) is detected to be cover art (with mime type `image/jpeg` or `image/png`, and name
`cover.{png,jpg}`), then it is simply replaced outright -- the old cover art will not be extracted and re-attached. This prevents
cover art attachments from "accumulating" when `mkvtaginator` is run mutliple times on the same file. To disable this behaviour,
use `--no-smart-replace-cover-art`.


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

TV information is provided by TheTVDB.com, but `mkvtaginator` is not endorsed or certified by TheTVDB.com or its affiliates. `mkvtaginator`
also uses the TMDb API but is not endorsed or certified by TMDb.









