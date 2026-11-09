# 🎬 mrename

> A beautiful command-line tool to mass rename your TV series files with smart pattern recognition

## ✨ Features

- 🔍 **Smart Pattern Detection** - Automatically detects 10 different naming patterns
- 📊 **Visual Analysis** - Beautiful colored tables showing match rates
- 🎯 **High Accuracy** - Pattern matching with detailed statistics
- 🚀 **Fast Performance** - Scans thousands of files in seconds
- 💫 **Safe Preview** - See changes before applying them
- 🎨 **Beautiful Interface** - Colorful terminal output with emojis

## 🚀 Quick Start

```bash
# Compile the program
make

# Run in current directory
./mrename

# Or specify a directory
./mrename /path/to/your/tv/shows
```

## 🤖 Non-interactive mode

Give `--pattern` and `--name` and mrename runs without asking anything: one plain line per file,
then a summary. Handy in scripts, cron jobs or [Canopy](https://github.com/AlexArtaud-Dev/Canopy).

```bash
# See what would change
./mrename --pattern auto --name "The Office" --dry-run /path/to/the/office

# Do it
./mrename --pattern auto --name "The Office" /path/to/the/office
```

| Option | Meaning |
|--------|---------|
| `-p, --pattern PATTERN` | `auto` (the pattern matching the most files), a number or an id from `--list-patterns` |
| `-n, --name NAME` | Series name used in the new file names |
| `-d, --dry-run` | Show what would be renamed, change nothing |
| `-l, --list-patterns` | List the patterns with their numbers and ids |
| `-V, --version` | Print the version |

Existing files are never overwritten: if the new name is taken, or two files would get the same
name, the file is left as is and reported as failed. Exit status: `0` done, `1` a file could not
be renamed or no pattern matched, `2` bad usage. Colors are only used in a terminal (and never
with `NO_COLOR` set).

## 📦 Releases

Pushing a tag (`git tag v1.1.0 && git push origin v1.1.0`) builds static binaries for
`linux-amd64` and `linux-arm64` and publishes them with `SHA256SUMS` and `canopy-plugin.yaml`,
so the release can be installed from Canopy's Plugins page.

## 📖 How It Works

1. **Scan** 📁 - Finds all video files in your directory
2. **Analyze** 🔍 - Tests 10 different naming patterns
3. **Choose** 🎯 - Pick the best matching pattern
4. **Preview** 👀 - See exactly what will be renamed
5. **Rename** ✨ - Transform your messy files into clean names

## 🎭 Supported Patterns

| Pattern | Example | Description |
|---------|---------|-------------|
| Standard | `S01E05` | Classic format |
| Alternative | `1x05` | Alternative numbering |
| Episode Only | `Episode 5` | Just episode numbers |
| And 7 more! | | Covers almost any format |

## 📁 Supported Video Formats

`.mkv` `.mp4` `.avi` `.mov` `.wmv` `.flv` `.webm` `.m4v` `.mpg` `.mpeg` `.ts` `.m2ts`

## 💡 Example

**Before:**
```
The.Office.S01E01.720p.WEB-DL.x264.mkv
the_office_1x02_hdtv.avi
TheOffice-S1E3-HDTV.mp4
```

**After:**
```
The Office S01E01.mkv
The Office S01E02.avi
The Office S01E03.mp4
```

## 🛠️ Requirements

- GCC compiler
- Linux/Unix system
- Terminal with color support

## 📜 License

Feel free to use and modify! 🎉

---

<div align="center">
Made with ❤️ for TV show enthusiasts
</div>
