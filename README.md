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
gcc -o mrename mrename.c

# Run in current directory
./mrename

# Or specify a directory
./mrename /path/to/your/tv/shows
```

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
