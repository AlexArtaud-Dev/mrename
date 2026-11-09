#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <regex.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#ifndef MRENAME_VERSION
#define MRENAME_VERSION "dev"
#endif

#define MAX_PATH 4096
#define MAX_NAME 200
#define MAX_FILES 10000
#define MAX_PATTERNS 10
#define MAX_MATCHES 10

// ANSI color codes
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"

// Clear screen and move cursor
#define CLEAR_SCREEN "\033[2J\033[H"
#define MOVE_UP(n) printf("\033[%dA", n)
#define MOVE_DOWN(n) printf("\033[%dB", n)
#define CLEAR_LINE "\033[K"

// Colors and screen control only when writing to a terminal (and NO_COLOR is not set)
static int use_color = 0;
#define C(code) (use_color ? (code) : "")

// Pattern structure
typedef struct {
    const char *id;
    const char *name;
    const char *regex;
    const char *description;
    int has_season;
} Pattern;

// File info structure
typedef struct {
    char *original_path;
    char *basename;
    char *new_name;
    int season;
    int episode;
    int matched;
} FileInfo;

// Pattern match results
typedef struct {
    int total_matches;
    double match_percentage;
} PatternResult;

// Global patterns
const Pattern patterns[] = {
    {"standard", "Standard", "S([0-9]+)E([0-9]+)", "S01E05, S1E12", 1},
    {"case-insensitive", "Case Insensitive", "[Ss]([0-9]+)[Ee]([0-9]+)", "s01e05, S01E05", 1},
    {"alternative", "Alternative", "([0-9]+)x([0-9]+)", "1x05, 01x12", 1},
    {"verbose", "Verbose", "Season\\.([0-9]+)\\.Episode\\.([0-9]+)", "Season.1.Episode.5", 1},
    {"flexible", "Flexible", "[Ss]eason[[:space:]._-]*([0-9]+)[[:space:]._-]*[Ee]pisode[[:space:]._-]*([0-9]+)", "Season 1 Episode 5", 1},
    {"episode", "Episode Only", "Episode[[:space:]._-]*([0-9]+)", "Episode.5, Episode 12", 0},
    {"short-episode", "Short Episode", "[Ee]p[[:space:]._-]*([0-9]+)", "Ep.5, ep 12", 0},
    {"minimal-episode", "Minimal Episode", "[Ee]([0-9]+)", "E05, e12", 0},
    {"dot", "Dot Notation", "\\.([0-9]+)\\.", ".1204.", 0},
    {"combined", "Combined", "([0-9]{1,2})([0-9]{2})", "105 (S1E05), 1205 (S12E05)", 1}
};

const int num_patterns = sizeof(patterns) / sizeof(patterns[0]);

// Function to print colored text
void print_color(const char *color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("%s", C(color));
    vprintf(format, args);
    printf("%s", C(RESET));
    va_end(args);
}

// Function to draw a horizontal line
void draw_line(int width, const char *color) {
    printf("%s", C(color));
    for (int i = 0; i < width; i++) {
        printf("━");
    }
    printf("%s\n", C(RESET));
}

// Reads a captured number, capped so that long digit runs cannot overflow the buffer
static int captured_number(const char *text, regmatch_t match) {
    char digits[10];
    int len = match.rm_eo - match.rm_so;
    if (len <= 0) return 0;
    if (len > (int)sizeof(digits) - 1) len = sizeof(digits) - 1;
    memcpy(digits, text + match.rm_so, len);
    digits[len] = '\0';
    return atoi(digits);
}

// Function to draw a box
void draw_box(const char *title, int width) {
    print_color(CYAN, "┏");
    int title_len = strlen(title);
    for (int i = 0; i < width - 2; i++) {
        if (i == (width - title_len) / 2 - 1) {
            printf(" %s ", title);
            i += title_len + 1;
        } else {
            printf("━");
        }
    }
    print_color(CYAN, "┓\n");
}

void draw_box_bottom(int width) {
    print_color(CYAN, "┗");
    for (int i = 0; i < width - 2; i++) {
        printf("━");
    }
    print_color(CYAN, "┛\n");
}

// Check if file has video extension
int is_video_file(const char *filename) {
    const char *extensions[] = {
        ".mkv", ".mp4", ".avi", ".mov", ".wmv", ".flv", 
        ".webm", ".m4v", ".mpg", ".mpeg", ".ts", ".m2ts"
    };
    int num_ext = sizeof(extensions) / sizeof(extensions[0]);
    
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    
    // Convert to lowercase for comparison
    char lower_ext[10];
    int i;
    for (i = 0; ext[i] && i < 9; i++) {
        lower_ext[i] = tolower(ext[i]);
    }
    lower_ext[i] = '\0';
    
    for (i = 0; i < num_ext; i++) {
        if (strcmp(lower_ext, extensions[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int compare_files(const void *a, const void *b) {
    return strcmp(((const FileInfo *)a)->basename, ((const FileInfo *)b)->basename);
}

// Scan directory for video files
int scan_directory(const char *dir_path, FileInfo **files) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char full_path[MAX_PATH];
    int count = 0;
    
    *files = malloc(MAX_FILES * sizeof(FileInfo));
    if (!*files) {
        perror("Memory allocation failed");
        return -1;
    }
    
    dir = opendir(dir_path);
    if (!dir) {
        perror("Failed to open directory");
        free(*files);
        return -1;
    }
    
    while ((entry = readdir(dir)) != NULL && count < MAX_FILES) {
        if (entry->d_name[0] == '.') continue;
        
        snprintf(full_path, MAX_PATH, "%s/%s", dir_path, entry->d_name);
        
        if (stat(full_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
            if (is_video_file(entry->d_name)) {
                (*files)[count].original_path = strdup(full_path);
                (*files)[count].basename = strdup(entry->d_name);
                (*files)[count].new_name = NULL;
                (*files)[count].matched = 0;
                count++;
            }
        }
    }
    
    closedir(dir);
    // Directory order is arbitrary; a stable order makes previews and logs predictable
    qsort(*files, count, sizeof(FileInfo), compare_files);
    return count;
}

// Test pattern against files
PatternResult test_pattern(const Pattern *pattern, FileInfo *files, int file_count) {
    regex_t regex;
    regmatch_t matches[MAX_MATCHES];
    PatternResult result = {0, 0.0};
    
    if (regcomp(&regex, pattern->regex, REG_EXTENDED) != 0) {
        return result;
    }
    
    for (int i = 0; i < file_count; i++) {
        if (regexec(&regex, files[i].basename, MAX_MATCHES, matches, 0) == 0) {
            files[i].matched = 1;
            files[i].season = 0;
            files[i].episode = 0;
            result.total_matches++;

            if (pattern->has_season && matches[1].rm_so != -1 && matches[2].rm_so != -1) {
                files[i].season = captured_number(files[i].basename, matches[1]);
                files[i].episode = captured_number(files[i].basename, matches[2]);
            } else if (!pattern->has_season && matches[1].rm_so != -1) {
                files[i].episode = captured_number(files[i].basename, matches[1]);
            }
        } else {
            files[i].matched = 0;
        }
    }
    
    regfree(&regex);
    result.match_percentage = file_count > 0 ? (double)result.total_matches / file_count * 100.0 : 0.0;
    return result;
}

// Generate new filename
void generate_new_name(FileInfo *file, const char *series_name) {
    char clean_name[MAX_NAME + 1];
    snprintf(clean_name, sizeof(clean_name), "%s", series_name);

    // Clean series name: no path separators, reserved or control characters
    for (int i = 0; clean_name[i]; i++) {
        if (strchr("<>:\"/\\|?*", clean_name[i]) || iscntrl((unsigned char)clean_name[i])) {
            clean_name[i] = '_';
        }
    }
    
    char *ext = strrchr(file->basename, '.');
    char new_name[MAX_PATH];
    
    if (file->season > 0) {
        snprintf(new_name, MAX_PATH, "%s S%02dE%02d%s", 
                 clean_name, file->season, file->episode, ext ? ext : "");
    } else {
        snprintf(new_name, MAX_PATH, "%s E%04d%s", 
                 clean_name, file->episode, ext ? ext : "");
    }
    
    file->new_name = strdup(new_name);
}

// Display pattern analysis table
void display_pattern_analysis(PatternResult *results, int file_count) {
    printf("\n");
    draw_box("PATTERN ANALYSIS", 80);
    
    printf("┃ %-3s ┃ %-25s ┃ %-25s ┃ %8s ┃ %6s ┃\n", 
           "No", "Pattern", "Example", "Matches", "Rate");
    print_color(CYAN, "┣━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━╋━━━━━━━━┫\n");
    
    int best_pattern = -1;
    double best_percentage = 0.0;
    
    for (int i = 0; i < num_patterns; i++) {
        char rate[20];
        snprintf(rate, sizeof(rate), "%.1f%%", results[i].match_percentage);
        
        const char *color = RESET;
        if (results[i].match_percentage >= 80) color = GREEN;
        else if (results[i].match_percentage >= 50) color = YELLOW;
        else if (results[i].match_percentage > 0) color = RED;
        else color = DIM;
        
        printf("┃ %s%-3d%s ┃ %-25s ┃ %s%-25s%s ┃ %s%8d%s ┃ %s%6s%s ┃\n",
               C(color), i + 1, C(RESET),
               patterns[i].name,
               C(DIM), patterns[i].description, C(RESET),
               C(color), results[i].total_matches, C(RESET),
               C(color), rate, C(RESET));
        
        if (results[i].match_percentage > best_percentage) {
            best_percentage = results[i].match_percentage;
            best_pattern = i;
        }
    }
    
    draw_box_bottom(80);
    
    printf("\n");
    print_color(BOLD GREEN, "📊 Total files scanned: %d\n", file_count);
    if (best_pattern >= 0 && best_percentage > 0) {
        print_color(BOLD CYAN, "✨ Best match: Pattern %d (%s) with %.1f%% coverage\n", 
                    best_pattern + 1, patterns[best_pattern].name, best_percentage);
    }
}

// Display rename preview
void display_rename_preview(FileInfo *files, int file_count) {
    printf("\n");
    draw_box("RENAME PREVIEW", 100);
    
    int shown = 0;
    for (int i = 0; i < file_count && shown < 10; i++) {
        if (files[i].matched && files[i].new_name) {
            printf("┃ ");
            print_color(YELLOW, "%-45s", files[i].basename);
            print_color(CYAN, " → ");
            print_color(GREEN, "%-45s", files[i].new_name);
            printf(" ┃\n");
            shown++;
        }
    }
    
    if (shown == 0) {
        printf("┃ %-96s ┃\n", "No files to rename");
    }
    
    draw_box_bottom(100);
    
    int total_matched = 0;
    for (int i = 0; i < file_count; i++) {
        if (files[i].matched) total_matched++;
    }
    
    if (total_matched > shown) {
        print_color(YELLOW, "\n... and %d more files\n", total_matched - shown);
    }
}

// Outcome of renaming one file
typedef enum {
    RENAME_DONE,
    RENAME_UNCHANGED,  // already has the target name
    RENAME_EXISTS,     // another file already has the target name: never overwritten
    RENAME_DUPLICATE,  // an earlier file in this batch gets the same name
    RENAME_ERROR       // the system refused (errno set)
} RenameOutcome;

// Another matched file before this one is given the same new name
static int duplicate_target(FileInfo *files, int index) {
    for (int i = 0; i < index; i++) {
        if (files[i].matched && files[i].new_name && strcmp(files[i].new_name, files[index].new_name) == 0) {
            return 1;
        }
    }
    return 0;
}

// Renames one file in place; with dry_run, only says what would happen
static RenameOutcome rename_one(FileInfo *files, int index, int dry_run) {
    FileInfo *file = &files[index];
    char new_path[MAX_PATH];
    struct stat st;

    if (strcmp(file->basename, file->new_name) == 0) return RENAME_UNCHANGED;
    if (duplicate_target(files, index)) return RENAME_DUPLICATE;

    char *dir = strdup(file->original_path);
    if (!dir) {
        errno = ENOMEM;
        return RENAME_ERROR;
    }
    char *last_slash = strrchr(dir, '/');
    if (last_slash) *last_slash = '\0';
    snprintf(new_path, MAX_PATH, "%s/%s", dir, file->new_name);
    free(dir);

    if (lstat(new_path, &st) == 0) return RENAME_EXISTS;
    if (dry_run) return RENAME_DONE;
    return rename(file->original_path, new_path) == 0 ? RENAME_DONE : RENAME_ERROR;
}

// Perform the actual renaming
int perform_rename(FileInfo *files, int file_count) {
    int success = 0, failed = 0;

    printf("\n");
    draw_line(80, CYAN);
    print_color(BOLD CYAN, "Renaming files...\n");
    draw_line(80, CYAN);

    for (int i = 0; i < file_count; i++) {
        if (files[i].matched && files[i].new_name) {
            RenameOutcome outcome = rename_one(files, i, 0);
            if (outcome == RENAME_DONE) {
                print_color(GREEN, "✓ ");
                printf("Renamed: %s\n", files[i].new_name);
                success++;
            } else if (outcome == RENAME_UNCHANGED) {
                print_color(DIM, "= Already named: %s\n", files[i].basename);
            } else {
                print_color(RED, "✗ ");
                printf("Failed: %s (%s)\n", files[i].basename,
                       outcome == RENAME_EXISTS ? "target exists" :
                       outcome == RENAME_DUPLICATE ? "same name as another file" : strerror(errno));
                failed++;
            }
        }
    }

    printf("\n");
    print_color(BOLD GREEN, "✓ Success: %d files\n", success);
    if (failed > 0) {
        print_color(BOLD RED, "✗ Failed: %d files\n", failed);
    }

    return failed;
}

// Free allocated memory
void cleanup(FileInfo *files, int file_count) {
    for (int i = 0; i < file_count; i++) {
        free(files[i].original_path);
        free(files[i].basename);
        if (files[i].new_name) free(files[i].new_name);
    }
    free(files);
}

// Pattern from --pattern: "auto" (-2), a number 1..N or an id; -1 when unknown
static int find_pattern(const char *arg) {
    if (strcmp(arg, "auto") == 0) return -2;
    char *end;
    errno = 0;
    long number = strtol(arg, &end, 10);
    if (errno == 0 && *arg && *end == '\0') {
        return (number >= 1 && number <= num_patterns) ? (int)number - 1 : -1;
    }
    for (int i = 0; i < num_patterns; i++) {
        if (strcmp(arg, patterns[i].id) == 0) return i;
    }
    return -1;
}

// The pattern matching the most files (the first one on a tie); -1 when none matches
static int best_pattern(const PatternResult *results) {
    int best = -1;
    for (int i = 0; i < num_patterns; i++) {
        if (results[i].total_matches > 0 && (best < 0 || results[i].total_matches > results[best].total_matches)) {
            best = i;
        }
    }
    return best;
}

static void list_patterns(void) {
    for (int i = 0; i < num_patterns; i++) {
        printf("%2d  %-16s  %-18s  %s\n", i + 1, patterns[i].id, patterns[i].name, patterns[i].description);
    }
}

static void usage(const char *program) {
    printf("Usage: %s [options] [directory]\n\n", program);
    printf("Without --pattern and --name, mrename asks interactively.\n\n");
    printf("  -p, --pattern PATTERN  auto (best match), a number or an id from --list-patterns\n");
    printf("  -n, --name NAME        series name used for the new file names\n");
    printf("  -d, --dry-run          show what would be renamed, change nothing\n");
    printf("  -l, --list-patterns    list the naming patterns and exit\n");
    printf("  -V, --version          print the version and exit\n");
    printf("  -h, --help             show this help and exit\n\n");
    printf("Exit status: 0 done, 1 a file could not be renamed or no pattern matched, 2 bad usage.\n");
}

// Non-interactive run: one plain line per file, then a summary. Never overwrites a file.
static int run_batch(const char *dir_path, const char *pattern_arg, const char *series_name, int dry_run) {
    FileInfo *files = NULL;
    int selected = find_pattern(pattern_arg);
    if (selected == -1) {
        fprintf(stderr, "mrename: unknown pattern '%s' (see --list-patterns)\n", pattern_arg);
        return 2;
    }
    if (series_name[0] == '\0' || strlen(series_name) > MAX_NAME) {
        fprintf(stderr, "mrename: the series name must be 1 to %d characters\n", MAX_NAME);
        return 2;
    }

    int file_count = scan_directory(dir_path, &files);
    if (file_count < 0) return 1;
    if (file_count == 0) {
        printf("No video files found in %s\n", dir_path);
        free(files);
        return 0;
    }

    if (selected == -2) {
        PatternResult results[MAX_PATTERNS];
        for (int i = 0; i < num_patterns; i++) {
            results[i] = test_pattern(&patterns[i], files, file_count);
        }
        selected = best_pattern(results);
        if (selected < 0) {
            fprintf(stderr, "mrename: no pattern matches these files\n");
            cleanup(files, file_count);
            return 1;
        }
    }

    PatternResult result = test_pattern(&patterns[selected], files, file_count);
    printf("Pattern %s (%s): %d of %d video files match\n",
           patterns[selected].id, patterns[selected].description, result.total_matches, file_count);

    int renamed = 0, unchanged = 0, skipped = 0, failed = 0;
    for (int i = 0; i < file_count; i++) {
        if (!files[i].matched) {
            printf("skipped (no match): %s\n", files[i].basename);
            skipped++;
            continue;
        }
        generate_new_name(&files[i], series_name);
        switch (rename_one(files, i, dry_run)) {
        case RENAME_DONE:
            printf("%s: %s -> %s\n", dry_run ? "would rename" : "renamed", files[i].basename, files[i].new_name);
            renamed++;
            break;
        case RENAME_UNCHANGED:
            printf("unchanged: %s\n", files[i].basename);
            unchanged++;
            break;
        case RENAME_EXISTS:
            printf("failed (target exists): %s -> %s\n", files[i].basename, files[i].new_name);
            failed++;
            break;
        case RENAME_DUPLICATE:
            printf("failed (same name as another file): %s -> %s\n", files[i].basename, files[i].new_name);
            failed++;
            break;
        case RENAME_ERROR:
            printf("failed (%s): %s\n", strerror(errno), files[i].basename);
            failed++;
            break;
        }
    }

    printf("%s: %d %s, %d unchanged, %d skipped, %d failed\n",
           dry_run ? "Dry run" : "Done", renamed, dry_run ? "would be renamed" : "renamed",
           unchanged, skipped, failed);
    cleanup(files, file_count);
    return failed > 0 ? 1 : 0;
}

// Main program
int main(int argc, char *argv[]) {
    char *dir_path = ".";
    char series_name[256];
    FileInfo *files = NULL;
    int file_count;
    const char *pattern_arg = NULL;
    const char *name_arg = NULL;
    int dry_run = 0;

    use_color = isatty(STDOUT_FILENO) && getenv("NO_COLOR") == NULL;

    static const struct option options[] = {
        {"pattern", required_argument, NULL, 'p'},
        {"name", required_argument, NULL, 'n'},
        {"dry-run", no_argument, NULL, 'd'},
        {"list-patterns", no_argument, NULL, 'l'},
        {"version", no_argument, NULL, 'V'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };
    int option;
    while ((option = getopt_long(argc, argv, "p:n:dlVh", options, NULL)) != -1) {
        switch (option) {
        case 'p': pattern_arg = optarg; break;
        case 'n': name_arg = optarg; break;
        case 'd': dry_run = 1; break;
        case 'l': list_patterns(); return 0;
        case 'V': printf("mrename %s\n", MRENAME_VERSION); return 0;
        case 'h': usage(argv[0]); return 0;
        default: usage(argv[0]); return 2;
        }
    }
    if (optind < argc) dir_path = argv[optind++];
    if (optind < argc) {
        fprintf(stderr, "mrename: only one directory can be given\n");
        return 2;
    }

    // Check if directory exists
    struct stat st;
    if (stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "mrename: directory '%s' does not exist\n", dir_path);
        return 1;
    }

    if (pattern_arg || name_arg || dry_run) {
        if (!pattern_arg || !name_arg) {
            fprintf(stderr, "mrename: --pattern and --name go together (--dry-run needs both)\n");
            return 2;
        }
        return run_batch(dir_path, pattern_arg, name_arg, dry_run);
    }

    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "mrename: not in a terminal; give --pattern and --name (see --help)\n");
        return 2;
    }

    // Clear screen and show header
    printf("%s", C(CLEAR_SCREEN));
    print_color(BOLD MAGENTA, "\n🎬 TV SERIES FILE RENAMER %s\n", MRENAME_VERSION);
    print_color(DIM WHITE, "High-performance batch renaming with pattern recognition\n\n");

    print_color(CYAN, "📁 Target directory: %s\n", dir_path);

    // Scan directory
    print_color(YELLOW, "\n⏳ Scanning directory...");
    fflush(stdout);

    clock_t start = clock();
    file_count = scan_directory(dir_path, &files);
    clock_t end = clock();
    double scan_time = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("\r%s", C(CLEAR_LINE));

    if (file_count <= 0) {
        print_color(RED, "No video files found in directory!\n");
        if (file_count == 0) free(files);
        return 1;
    }

    print_color(GREEN, "✓ Found %d video files in %.3f seconds\n", file_count, scan_time);

    // Test all patterns
    print_color(YELLOW, "🔍 Analyzing file patterns...\n");

    PatternResult results[MAX_PATTERNS];
    for (int i = 0; i < num_patterns; i++) {
        results[i] = test_pattern(&patterns[i], files, file_count);
    }

    // Display analysis table
    display_pattern_analysis(results, file_count);

    // Get user input for pattern selection
    int selected_pattern;
    while (1) {
        print_color(YELLOW, "\n📌 Select pattern (1-%d, 0 to quit): ", num_patterns);
        int read = scanf("%d", &selected_pattern);
        if (read == EOF) {
            cleanup(files, file_count);
            return 1;
        }
        if (read != 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF); // Clear input buffer
            continue;
        }

        if (selected_pattern == 0) {
            print_color(YELLOW, "Exiting...\n");
            cleanup(files, file_count);
            return 0;
        }

        if (selected_pattern >= 1 && selected_pattern <= num_patterns) {
            selected_pattern--; // Convert to 0-based index
            break;
        }

        print_color(RED, "Invalid selection! Please choose 1-%d or 0 to quit.\n", num_patterns);
    }

    // Get series name
    print_color(YELLOW, "\n📝 Enter series name: ");
    int c;
    while ((c = getchar()) != '\n' && c != EOF); // Clear input buffer
    if (!fgets(series_name, sizeof(series_name), stdin)) {
        print_color(RED, "Failed to read series name!\n");
        cleanup(files, file_count);
        return 1;
    }
    series_name[strcspn(series_name, "\n")] = '\0'; // Remove newline

    if (strlen(series_name) == 0 || strlen(series_name) > MAX_NAME) {
        print_color(RED, "The series name must be 1 to %d characters!\n", MAX_NAME);
        cleanup(files, file_count);
        return 1;
    }

    // Apply selected pattern and generate new names
    test_pattern(&patterns[selected_pattern], files, file_count);
    for (int i = 0; i < file_count; i++) {
        if (files[i].matched) {
            generate_new_name(&files[i], series_name);
        }
    }

    // Show preview
    display_rename_preview(files, file_count);

    // Confirm rename
    int failed = 0;
    print_color(BOLD YELLOW, "\n⚠️  Proceed with renaming? (y/N): ");
    int confirm = getchar();

    if (confirm == 'y' || confirm == 'Y') {
        failed = perform_rename(files, file_count);
    } else {
        print_color(YELLOW, "Renaming cancelled.\n");
    }

    // Cleanup
    cleanup(files, file_count);

    print_color(DIM WHITE, "\n👋 Thank you for using TV Series Renamer!\n\n");

    return failed > 0 ? 1 : 0;
}
