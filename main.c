#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <ctype.h>

#define MAX_PATH_LENGTH 1024

int ignore_case = 0;  // Флаг для игнорирования регистра

// Функция проверяет, является ли файл текстовым
int is_text_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) return 0;
    
    // Простая проверка расширения файла
    return strcmp(ext, ".txt") == 0 || 
           strcmp(ext, ".c") == 0 ||
           strcmp(ext, ".h") == 0 ||
           strcmp(ext, ".cpp") == 0 ||
           strcmp(ext, ".java") == 0;
}

// Функция сравнения строк с учетом/без учета регистра
int compare_strings(const char *s1, const char *s2) {
    if (ignore_case) {
        while (*s1 && *s2) {
            if (tolower(*s1) != tolower(*s2)) break;
            s1++;
            s2++;
        }
        return tolower(*s1) - tolower(*s2);
    } else {
        return strcmp(s1, s2);
    }
}

// Функция поиска слова в содержимом файла
void search_in_content(const char *full_path, const char *content, size_t content_length, const char *word) {
    int line_number = 1;
    const char *line_start = content;
    const char *line_end;
    
    while ((line_end = memchr(line_start, '\n', content_length - (line_start - content)))) {
        size_t line_length = line_end - line_start;
        
        // Выделяем память под строку и копируем ее
        char *line = malloc(line_length + 1);
        strncpy(line, line_start, line_length);
        line[line_length] = '\0';
        
        // Поиск слова в строке
        char *pos = strstr(line, word);
        if (pos != NULL) {
            printf("Файл: %s\n", full_path);
            printf("Строка %d: %s\n", line_number, line);
            printf("------------------------------\n");
        }
        
        free(line);
        line_start = line_end + 1;
        line_number++;
    }
}

// Функция поиска слов в файле с использованием mmap
void search_in_file(const char *full_path, const char *word) {
    int fd = open(full_path, O_RDONLY);
    if (fd == -1) {
        perror("Ошибка открытия файла");
        return;
    }

    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1) {
        perror("Ошибка получения информации о файле");
        close(fd);
        return;
    }

    // Отображаем файл в память
    char *mapped;
    mapped = mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        perror("Ошибка отображения файла в память");
        close(fd);
        return;
    }

    // Ищем слово в отображенном файле
    if (strstr(mapped, word) != NULL) {
        FILE *fp = fopen(full_path, "r");
        if (fp) {
            char line[1024];
            int line_number = 1;
            
            while (fgets(line, sizeof(line), fp)) {
                char *newline = strchr(line, '\n');
                if (newline) *newline = '\0';
                
                if (strstr(line, word) != NULL) {
                    printf("Файл: %s\n", full_path);
                    printf("Строка %d: %s\n", line_number, line);
                    printf("------------------------------\n");
                }
                
                line_number++;
            }
            
            fclose(fp);
        }
    }

    // Освобождаем память и закрываем дескрипторы
    munmap(mapped, file_stat.st_size);
    close(fd);
}

// Рекурсивная функция обработки директории
void process_directory(const char *path, const char *word) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("Не могу открыть директорию");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        // Пропускаем . и ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Формируем полный путь
        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Получаем информацию о файле
        struct stat path_stat;
        lstat(full_path, &path_stat);

        if (S_ISDIR(path_stat.st_mode)) {
            // Это директория, рекурсивно обрабатываем
            process_directory(full_path, word);
        } else if (S_ISREG(path_stat.st_mode)) {
            // Это обычный файл, проверяем, является ли он текстовым
            if (is_text_file(entry->d_name)) {
                search_in_file(full_path, word);
            }
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    // Проверка количества аргументов
    if (argc < 2) {
        fprintf(stderr, "Использование: %s [-i] <слово_для_поиска> [путь_к_директории]\n", argv[0]);
        return 1;
    }

    // Обработка флага -i
    int arg_index = 1;
    if (argc > 1 && strcmp(argv[1], "-i") == 0) {
        ignore_case = 1;
        arg_index++;
    }

    // Получаем слово для поиска
    char *search_word = argv[arg_index];

    // Получаем путь к директории
    char dir_path[MAX_PATH_LENGTH];
    if (arg_index + 1 < argc) {
        strncpy(dir_path, argv[arg_index + 1], MAX_PATH_LENGTH);
    } else {
        const char *home = getenv("HOME");
        snprintf(dir_path, MAX_PATH_LENGTH, "%s/files", home ? home : "~");
    }

    // Начинаем поиск
    printf("Поиск слова \"%s\" в директории %s (и поддиректориях):\n\n", search_word, dir_path);
    process_directory(dir_path, search_word);

    return 0;
}
