#define _GNU_SOURCE
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

#define MAX_PATH 4096
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

// Pattern structure
typedef struct {
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
    {"Standard", "S([0-9]+)E([0-9]+)", "S01E05, S1E12", 1},
    {"Case Insensitive", "[Ss]([0-9]+)[Ee]([0-9]+)", "s01e05, S01E05", 1},
    {"Alternative", "([0-9]+)x([0-9]+)", "1x05, 01x12", 1},
    {"Verbose", "Season\\.([0-9]+)\\.Episode\\.([0-9]+)", "Season.1.Episode.5", 1},
    {"Flexible", "[Ss]eason[\\.\\s]*([0-9]+)[\\.\\s]*[Ee]pisode[\\.\\s]*([0-9]+)", "Season 1 Episode 5", 1},
    {"Episode Only", "Episode[\\.\\s]*([0-9]+)", "Episode.5, Episode 12", 0},
    {"Short Episode", "[Ee]p[\\.\\s]*([0-9]+)", "Ep.5, ep 12", 0},
    {"Minimal Episode", "[Ee]([0-9]+)", "E05, e12", 0},
    {"Dot Notation", "\\.([0-9]+)\\.", ".1204.", 0},
    {"Combined", "([0-9]{1,2})([0-9]{2})", "105 (S1E05), 1205 (S12E05)", 1}
};

const int num_patterns = sizeof(patterns) / sizeof(patterns[0]);

// Function to print colored text
void print_color(const char *color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("%s", color);
    vprintf(format, args);
    printf("%s", RESET);
    va_end(args);
}

// Function to draw a horizontal line
void draw_line(int width, const char *color) {
    printf("%s", color);
    for (int i = 0; i < width; i++) {
        printf("━");
    }
    printf("%s\n", RESET);
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
            result.total_matches++;
            
            if (pattern->has_season && matches[1].rm_so != -1 && matches[2].rm_so != -1) {
                char season_str[10], episode_str[10];
                int len;
                
                len = matches[1].rm_eo - matches[1].rm_so;
                strncpy(season_str, files[i].basename + matches[1].rm_so, len);
                season_str[len] = '\0';
                files[i].season = atoi(season_str);
                
                len = matches[2].rm_eo - matches[2].rm_so;
                strncpy(episode_str, files[i].basename + matches[2].rm_so, len);
                episode_str[len] = '\0';
                files[i].episode = atoi(episode_str);
            } else if (!pattern->has_season && matches[1].rm_so != -1) {
                char episode_str[10];
                int len = matches[1].rm_eo - matches[1].rm_so;
                strncpy(episode_str, files[i].basename + matches[1].rm_so, len);
                episode_str[len] = '\0';
                files[i].season = 0;
                files[i].episode = atoi(episode_str);
            }
        } else {
            files[i].matched = 0;
        }
    }
    
    regfree(&regex);
    result.match_percentage = (double)result.total_matches / file_count * 100.0;
    return result;
}

// Generate new filename
void generate_new_name(FileInfo *file, const char *series_name) {
    char clean_name[256];
    strcpy(clean_name, series_name);
    
    // Clean series name
    for (int i = 0; clean_name[i]; i++) {
        if (strchr("<>:\"/\\|?*", clean_name[i])) {
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
               color, i + 1, RESET,
               patterns[i].name,
               DIM, patterns[i].description, RESET,
               color, results[i].total_matches, RESET,
               color, rate, RESET);
        
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

// Perform the actual renaming
int perform_rename(FileInfo *files, int file_count) {
    int success = 0, failed = 0;
    char new_path[MAX_PATH];
    
    printf("\n");
    draw_line(80, CYAN);
    print_color(BOLD CYAN, "Renaming files...\n");
    draw_line(80, CYAN);
    
    for (int i = 0; i < file_count; i++) {
        if (files[i].matched && files[i].new_name) {
            char *dir = strdup(files[i].original_path);
            char *last_slash = strrchr(dir, '/');
            if (last_slash) *last_slash = '\0';
            
            snprintf(new_path, MAX_PATH, "%s/%s", dir, files[i].new_name);
            
            if (rename(files[i].original_path, new_path) == 0) {
                print_color(GREEN, "✓ ");
                printf("Renamed: %s\n", files[i].new_name);
                success++;
            } else {
                print_color(RED, "✗ ");
                printf("Failed: %s\n", files[i].basename);
                failed++;
            }
            
            free(dir);
        }
    }
    
    printf("\n");
    print_color(BOLD GREEN, "✓ Success: %d files\n", success);
    if (failed > 0) {
        print_color(BOLD RED, "✗ Failed: %d files\n", failed);
    }
    
    return success;
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

// Main program
int main(int argc, char *argv[]) {
    char *dir_path = ".";
    char series_name[256];
    FileInfo *files = NULL;
    int file_count;
    
    // Clear screen and show header
    printf(CLEAR_SCREEN);
    print_color(BOLD MAGENTA, "\n🎬 TV SERIES FILE RENAMER v2.0\n");
    print_color(DIM WHITE, "High-performance batch renaming with pattern recognition\n\n");
    
    // Parse arguments
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            printf("Usage: %s [directory]\n", argv[0]);
            printf("  directory  Target directory (default: current directory)\n\n");
            return 0;
        }
        dir_path = argv[1];
    }
    
    // Check if directory exists
    struct stat st;
    if (stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        print_color(RED, "Error: Directory '%s' does not exist!\n", dir_path);
        return 1;
    }
    
    print_color(CYAN, "📁 Target directory: %s\n", dir_path);
    
    // Scan directory
    print_color(YELLOW, "\n⏳ Scanning directory...");
    fflush(stdout);
    
    clock_t start = clock();
    file_count = scan_directory(dir_path, &files);
    clock_t end = clock();
    double scan_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("\r" CLEAR_LINE);
    
    if (file_count <= 0) {
        print_color(RED, "No video files found in directory!\n");
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
        if (scanf("%d", &selected_pattern) != 1) {
            while (getchar() != '\n'); // Clear input buffer
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
    while (getchar() != '\n'); // Clear input buffer
    if (!fgets(series_name, sizeof(series_name), stdin)) {
        print_color(RED, "Failed to read series name!\n");
        cleanup(files, file_count);
        return 1;
    }
    series_name[strcspn(series_name, "\n")] = '\0'; // Remove newline
    
    if (strlen(series_name) == 0) {
        print_color(RED, "Series name cannot be empty!\n");
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
    char confirm;
    print_color(BOLD YELLOW, "\n⚠️  Proceed with renaming? (y/N): ");
    confirm = getchar();
    
    if (confirm == 'y' || confirm == 'Y') {
        perform_rename(files, file_count);
    } else {
        print_color(YELLOW, "Renaming cancelled.\n");
    }
    
    // Cleanup
    cleanup(files, file_count);
    
    print_color(DIM WHITE, "\n👋 Thank you for using TV Series Renamer!\n\n");
    
    return 0;
}
