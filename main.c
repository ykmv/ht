#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>

// TODO: If there are no habits `ht -l` will segfault if `ht` was compiled
//       with `tcc`.
// TODO: support for multiple date formats (YYYY.MM.DD/DD.MM.YYYY/MM.DD.YYYY)
//       by the means of env variable
// TODO: as of now, it is very difficult to select a habit 
//       (maybe use `fzf' if it is present in the system?)
// TODO: Add a check for ANSI colours support
// TODO: add -y to the -c argument to allow to see a specific year
// TODO: Add quantity to the habit (i.e. how many times the habit needs to be
//       completed in a day)
// TODO: Add frequency to the habit (i.e. once per day once per week, once per
//       month, etc.)

// A Habit Tracker
//
// Each habit is a separate CSV file with a name of habit
// as the name of the file. Structure of CSV:
//   Day,Marked

extern int errno;
extern char *optarg;

#define HABIT_CREATE_ARG 'a'
#define HABIT_REMOVE_ARG 'r'
#define HABIT_LIST_ARG 'l'
#define HABIT_SELECT_DEFAULT_ARG 's'
#define HABIT_DESELECT_DEFAULT_ARG 'S'
#define HABIT_DISPLAY_ARG 'c'
#define HABIT_DISPLAY_AS_LIST_ARG 'C'
#define HABIT_NON_DEFAULT_ARG 'H'
#define HABIT_CUSTOM_DAY_ARG 'd'
#define PRINT_HELP_ARG 'h'
#define PRINT_VERSION_ARG 'v'
#define GRAPH_WIDTH_ARG 'w'
#define GRAPH_YEAR_ARG 'y'

#define ALLARGS "a:r:ls:Sc:C:H:hvy:w:d:"

#define ANSI_BLK "\e[0;30m"
#define ANSI_RED "\e[0;31m"
#define ANSI_GRN "\e[0;32m"
#define ANSI_YEL "\e[0;33m"
#define ANSI_BGRED "\e[41m"
#define ANSI_BGGRN "\e[42m"
#define ANSI_BGIBLK "\e[0;100m"
#define ANSI_BGGRY "\e[40m"
#define ANSI_RESET "\e[0m"
#define GRAPH_BLANK_EVEN 2
#define GRAPH_BLANK_ODD 3
#define GRAPH_XMAX 7
#define GRAPH_YMAX 39

#define DAY_UNMARKED 0
#define DAY_COMPLETE 1
#define DAY_FAILED  -1
typedef struct {
   signed int marked;
   time_t timestamp;
} Day;

typedef struct {
   int failed_days;
   int completed_days;
   time_t creation_timestamp;
} Stats;

typedef struct {
   char *name;
   Day *days;
   int days_count;
   Stats stats;
   int to_add_custom_days;
   int is_default;
} Habit;

const char *default_habit_filename = "default";
const char *default_habit_path = "/.local/share/ht";

const char *habit_folder_path_env_var = "HTDIR";
const char *date_format_env_var = "HTFORMAT";
const char *nerd_font_var = "HTNERD";
const char *mark_prev_unmarked_days_as_failed_env_var = "HTUNMARKEDTOFAIL";

int habit_file_create(Habit *habit);
int habit_file_write(Habit *habit, char *mode);
int habit_file_read(Habit *habit);
char *habitpath(int trailing_slash);
char *habit_make_filename(Habit *habit);
Day *habit_add_day(Habit *habit, time_t time, int mark);
Day *habit_add_today(Habit *habit, int mark);
int default_habit_write(Habit *habit);
char *default_habit_read();
char *default_habit_path_make();
time_t ymd_to_time_t(int year, int month, int day);
int date_compare(time_t ts1, time_t ts2);
void calc_stats(Habit *habit);
Day *day_exists(Habit *habit, time_t time);
void day_sort(Day arr[], int len);
char *day_mark_str(int day_mark);
void switch_day_mark(Day *day);
void print_help();
void print_version();
int file_exists(char *filename);
void graph_cell_print(int cell);
void fillout_skipped_days(Habit *habit);
void habit_display_as_list(Habit *habit, int is_default);
void habit_display_graph(Habit *habit, int is_default, int custom_width);

int
main(int argc, char *argv[]) {
   char *habit_path = habitpath(0);

   int graph_width = 0;
   int graph_year = 0;

   struct stat st = {0};
   int err = stat(habit_path, &st);
   if (err == -1) {
      mkdir(habit_path, 0700);
      printf("A storage for habits has been created at %s\n", habit_path);
   } else if ((st.st_mode & S_IFMT) != (S_IFDIR)) {
      printf("%s is not a directory nor a symlink\n", habit_path);
      return 1;
   }
   free(habit_path);


   if (argc <= 1) {
      Habit habit = {0};
      habit.name = NULL;
      char *default_path = default_habit_path_make();
      if (file_exists(default_path)) {
         print_help();
         free(default_path);
         return 0;
      }

      habit.name = default_habit_read();
      habit_file_read(&habit);

      int dc = (habit.days_count != 0) ?
         date_compare(habit.days[habit.days_count-1].timestamp, time(NULL)) : 2;

      if (dc) {
         habit_add_today(&habit, DAY_COMPLETE);
         printf("TODAY: %s\n",
                day_mark_str(habit.days[habit.days_count-1].marked));
      } else {
         int temp = habit.days[habit.days_count-1].marked;
         switch_day_mark(&habit.days[habit.days_count-1]);
         printf("TODAY: %s -> %s\n", 
                day_mark_str(temp),
                day_mark_str(habit.days[habit.days_count-1].marked));
      }

      fillout_skipped_days(&habit);

      calc_stats(&habit);
      habit_file_write(&habit, "w");
      free(habit.name);
      free(default_path);
      free(habit.days);
      return 0;
   }

   int habit_count = 0;
   int current_habit = 0;
   int to_add_days = 0;
   Habit *habits = NULL;
   char *default_path = default_habit_read();

   opterr = 1;
   char option;
   while ((option = getopt(argc, argv, ALLARGS)) != -1) { 
      switch (option) {
         case HABIT_CREATE_ARG: {
            Habit habit = {0};
            habit.name = optarg;
            habit.stats.creation_timestamp = time(NULL);

            if (habit_file_create(&habit)) {
               fprintf(stderr, "Habit %s already exists\n", habit.name);
            } else {
               printf("Habit %s created\n", habit.name);
            }
         } break;
         case HABIT_REMOVE_ARG: {
            Habit habit = {0};
            char *filename = "";
            habit.name = optarg;
            filename = habit_make_filename(&habit);
            if (file_exists(filename)) {
               fprintf(stderr, "%s does not exist\n", habit.name);
               free(filename);
            } else {
               // TODO: check if the deleted habit is actually a default one
               char choice = 0;
               printf("Are you sure you want to delete this habit?: [y/N] ");
               scanf("%c", &choice);
               if (choice == 'y' || choice == 'Y') {
                  if (!remove(filename))
                     printf("%s has been deleted successfully\n", habit.name);
                  else
                     fprintf(stderr, "failed to delete %s\n", habit.name);

                  char *default_habit_name = default_habit_read();
                  if (default_habit_name != NULL) {
                     char *default_habit_path = default_habit_path_make();
                     if (!remove(default_habit_path)) {
                        printf(ANSI_YEL "%s" ANSI_RESET
                           "was default, default habit is no longer set\n",
                           habit.name);
                     } else {
                        printf("failed to unset default habit\n");
                     }
                     free(default_habit_path);
                     free(default_habit_name);
                  }
               }
            }
         } break;
         case HABIT_LIST_ARG: {
            // TODO: segfaults when no habits exist
            struct dirent *de;
            char *habit_path = habitpath(0);
            DIR *dr = opendir(habit_path);
            if (dr == NULL) {
               perror("ERROR");
               free(habit_path);
            } else {
               Habit *list_habits;
               int lhabit_count = 0;
               while ((de = readdir(dr)) != NULL)
                  if (strcmp(de->d_name, "..")
                      && strcmp(de->d_name, ".")
                      && strcmp(de->d_name, "default")) {
                     if (lhabit_count == 0) {
                        lhabit_count = 1;
                        list_habits = calloc(1, sizeof(Habit));
                     } else {
                        lhabit_count++;
                        list_habits = realloc(list_habits, lhabit_count * sizeof(Habit));
                     }

                     list_habits[lhabit_count-1].name = calloc(
                        strlen(de->d_name)-3,
                        sizeof(char));
                     list_habits[lhabit_count-1].days_count = 0;
                     list_habits[lhabit_count-1].days = NULL;
                     list_habits[lhabit_count-1].stats.completed_days = 0;
                     list_habits[lhabit_count-1].stats.failed_days = 0;
                     list_habits[lhabit_count-1].stats.creation_timestamp = 0;

                     for (int i = 0; i < strlen(de->d_name)-4; i++)
                        list_habits[lhabit_count-1].name[i] = de->d_name[i];

                     habit_file_read(&list_habits[lhabit_count-1]);
                     printf("%s", list_habits[lhabit_count-1].name);
                     char *default_habit_name = default_habit_read();
                     int dhn = (default_habit_name != NULL) ?
                        !strcmp(default_habit_name, 
                                list_habits[lhabit_count-1].name)
                        : 0;
                     if (dhn) printf(" (default)");
                     free(default_habit_name);
                     if (list_habits[lhabit_count-1].days_count > 0) {
                        if (!date_compare(
                              list_habits[lhabit_count-1].
                                 days[list_habits[lhabit_count-1].days_count-1].timestamp,
                              time(NULL)))
                           printf(" (TODAY: %s)",
                                  day_mark_str(
                                    list_habits[lhabit_count-1].
                                    days[list_habits[lhabit_count-1].days_count-1].marked));
                        printf("\n");
                        printf("   Days "
                               ANSI_GRN "completed: %d; "
                               ANSI_RED "failed: %d; ",
                                  list_habits[lhabit_count-1].stats.completed_days,
                                  list_habits[lhabit_count-1].stats.failed_days);
                     } else printf("\n   ");
                     struct tm *ti = localtime(
                        &list_habits[lhabit_count-1].stats.creation_timestamp);
                     printf(ANSI_RESET "created %d.%02d.%02d\n", 
                            ti->tm_year + 1900,
                            ti->tm_mon + 1,
                            ti->tm_mday);
                     free(list_habits[lhabit_count-1].name);
                     free(list_habits[lhabit_count-1].days);
                  }
                  free(list_habits);
                  free(habit_path);
                  closedir(dr);
            } 
         } break;
         case HABIT_SELECT_DEFAULT_ARG: {
            Habit habit = {0};
            if (optarg != NULL) {
               habit.name = optarg;
               char *habit_path = habit_make_filename(&habit);
               if (file_exists(habit_path)) {
                  fprintf(stderr, "Habit " ANSI_YEL "%s" ANSI_RESET" doesn't exist\n", habit.name);
               } else if (default_habit_write(&habit)) {
                  fprintf(stderr, "Unable to write a default habit\n");
               } else {
                  printf("%s has been selected as a default habit\n", 
                         habit.name);
               }
               free(habit_path);
            } else {
               fprintf(stderr, "Please provide a habit name\n");
            }
         } break;
         case HABIT_DESELECT_DEFAULT_ARG: {
            char* default_path = "";
            if ((default_path = default_habit_read()) == NULL) {
               fprintf(stderr, "Default habit is not set, nothing to unselect\n");
            } else if (default_path != NULL) {
               char *default_habit_path_file = default_habit_path_make();
               remove(default_habit_path_file);
               free(default_habit_path_file);
               free(default_path);
               fprintf(stderr, "Default habit has been unselected\n");
            }
         } break;
         case PRINT_HELP_ARG: {
            print_help();
         } break;
         case PRINT_VERSION_ARG: {
            print_version();
         } break;
         case HABIT_NON_DEFAULT_ARG: {
            if (optarg == NULL) {
               fprintf(stderr, "No habit name was provided\n");
            } else {
               int habit_loaded = 0;
               for (int i = 0; i < habit_count; i++) {
                  if(!strcmp(optarg, habits[i].name)) {
                     current_habit = i;
                     habit_loaded = 1;
                  }
               }
               if (!habit_loaded) {
                  if (habit_count == 0) {
                     habit_count++;
                     habits = calloc(1, sizeof(Habit));
                  } else {
                     habit_count++;
                     habits = realloc(habits, habit_count * sizeof(Habit));
                  }
                  current_habit = habit_count-1;

                  habits[current_habit].days_count = 0;
                  habits[current_habit].days = NULL;
                  habits[current_habit].stats.completed_days = 0;
                  habits[current_habit].stats.failed_days = 0;
                  habits[current_habit].stats.creation_timestamp = 0;
                  habits[current_habit].name = optarg;
                  habits[current_habit].to_add_custom_days = 0;

                  habit_file_read(&habits[current_habit]);
               }
            }
         } break;
         case HABIT_CUSTOM_DAY_ARG: {
            time_t new_day_timestamp;
            int year, month, day;
            time_t time_now = time(NULL);
            struct tm *now = localtime(&time_now);

            if (habits == NULL) {
               if (default_path != NULL) {
                  habit_count++;
                  habits = calloc(1, sizeof(Habit));
                  habits[0].name = default_path;
                  habit_file_read(&habits[0]);
                  habits[0].is_default = 1;
               }
            }

            if (habit_count != 1 && !strcmp(default_path, habits[current_habit].name)) {
               // this prevents marking today in cases like 
               // ht -d 1 -H default_habit -d 1
               habits[current_habit].to_add_custom_days = 1;
               current_habit = 0;
            }

            if (habits != NULL) {
               habits[current_habit].to_add_custom_days = 1;
               if (optarg != NULL) {
                  if (sscanf(optarg, "%d.%d.%d", &year,&month,&day) == 3
                      && year > 1
                      && month >= 1 && month <= 12
                      && day >= 1 && day <= 31) {
                     new_day_timestamp = ymd_to_time_t(year, month, day);
                  } else if (sscanf(optarg, "%d.%d", &month, &day) == 2
                             && month >= 1 && month <= 12
                             && day >= 1 && day <= 31) {
                     new_day_timestamp = ymd_to_time_t(now->tm_year+1900, month, day);
                  } else if (sscanf(optarg, "%d", &day) == 1 && day >= 1 && day <= 31) {
                     new_day_timestamp = ymd_to_time_t(
                        now->tm_year+1900, now->tm_mon + 1, day);
                  } else {
                     fprintf(stderr, "The provided date \"%s\" cannot be parsed\n", optarg);
                  }
               } else {
                  fprintf(stderr, "Please provide a custom day\n");
               }

               struct tm *ti = localtime(&new_day_timestamp);
               if (habits[current_habit].days_count == 0) {
                  habit_add_day(&habits[current_habit], new_day_timestamp, DAY_COMPLETE);
                  printf("Habit " ANSI_YEL "\"%s\"" ANSI_RESET": %d.%02d.%02d: %s\n",
                         habits[current_habit].name,
                         ti->tm_year + 1900,
                         ti->tm_mon + 1,
                         ti->tm_mday,
                         day_mark_str(DAY_COMPLETE));
               } else {
                  Day *existing_day = day_exists(&habits[current_habit], 
                                                 new_day_timestamp);
                  if (existing_day != NULL) {
                     int temp = existing_day->marked;
                     switch_day_mark(existing_day);
                     printf("Habit " ANSI_YEL "\"%s\"" ANSI_RESET ": %d.%02d.%02d: %s -> %s\n", 
                            habits[current_habit].name,
                            ti->tm_year + 1900,
                            ti->tm_mon + 1,
                            ti->tm_mday,
                            day_mark_str(temp),
                            day_mark_str(existing_day->marked));
                  } else {
                     if (date_compare(time(NULL), new_day_timestamp) != -1) {
                        habit_add_day(
                           &habits[current_habit],
                           new_day_timestamp,
                           DAY_COMPLETE);
                        printf("Habit " ANSI_YEL "\"%s\"" ANSI_RESET": %d.%02d.%02d: %s\n",
                               habits[current_habit].name,
                               ti->tm_year + 1900,
                               ti->tm_mon + 1,
                               ti->tm_mday,
                               day_mark_str(DAY_COMPLETE));
                     } else {
                        printf("Habit " ANSI_YEL "\"%s\"" ANSI_RESET
                           ": the provided date %d.%02d.%02d is bigger than today\n",
                           habits[current_habit].name,
                           ti->tm_year + 1900,
                           ti->tm_mon + 1,
                           ti->tm_mday);
                     }

                     if (habits[current_habit].
                           days[habits[current_habit].days_count-2].timestamp >
                         habits[current_habit].
                           days[habits[current_habit].days_count-1].timestamp)
                        day_sort(habits[current_habit].days,
                                 habits[current_habit].days_count);
                  }
               } 
            } else {
               fprintf(stderr, 
                       "No habit name was provided (with `-H` arg) "
                       "nor default habit was selected (with `-s` arg)\n");
            }
         } break;
         case HABIT_DISPLAY_AS_LIST_ARG: {
            Habit habit = {0};
            int is_default = 0;
            if (optarg != NULL) {
               if (!strcmp(optarg, "-") || optarg[0] == '-') {
                  optind -= 1;
                  is_default++;
                  habit.name = default_habit_read();
               } else {
                  habit.name = optarg;
               }
            }
            habit_display_as_list(&habit, is_default);
         } break;
         case HABIT_DISPLAY_ARG: {
            Habit habit = {0};
            int is_default = 0;
            if (optarg != NULL) {
               if (!strcmp(optarg, "-") || optarg[0] == '-') {
                  optind -= 1;
                  is_default++;
                  habit.name = default_habit_read();
               } else {
                  habit.name = optarg;
               }
            }
            habit_display_graph(&habit, is_default, 30);
         } break;
         case '?': {
            switch(optopt) {
               case HABIT_DISPLAY_AS_LIST_ARG: {
                  Habit habit = {0};
                  habit.name = default_habit_read();
                  if (habit.name != NULL) {
                     habit_display_as_list(&habit, 1);
                     free(habit.name);
                  }
               } break;
               case HABIT_DISPLAY_ARG: {
                  Habit habit = {0};
                  habit.name = default_habit_read();
                  if (habit.name != NULL) {
                     habit_display_graph(&habit, 1, 30);
                     free(habit.name);
                  }
               } break;
            }
         } break;
      }
   }

   for (int i = 0; i < habit_count; i++) {
      if (!habits[i].to_add_custom_days) {
         int dc = (habits[i].days_count != 0) ?
            date_compare(habits[i].days[habits[i].days_count-1].timestamp, time(NULL)) : 2;

         if (dc) {
            habit_add_today(&habits[i], DAY_COMPLETE);
            printf("Habit " ANSI_YEL "\"%s\"" ANSI_RESET": TODAY: %s\n", 
                   habits[i].name,
                   day_mark_str(habits[i].days[habits[i].days_count-1].marked));
         } else {
            int temp = habits[i].days[habits[i].days_count-1].marked;
            switch_day_mark(&habits[i].days[habits[i].days_count-1]);
            printf("Habit " ANSI_YEL "\"%s\"" ANSI_RESET": TODAY: %s -> %s\n", 
                   habits[i].name,
                   day_mark_str(temp),
                   day_mark_str(habits[i].days[habits[i].days_count-1].marked));
         }

         fillout_skipped_days(&habits[i]);
      }
      calc_stats(&habits[i]);
      habit_file_write(&habits[i], "w");
      if (habits[i].days_count > 0) free(habits[i].days);
   }

   if (default_path != NULL) free(default_path);
   free(habits);

   return 0;
}

int
file_exists(char *filename) {
   struct stat st = {0};
   int err = stat(filename, &st);
   if (err == -1) {
      return 1;
   } else {
      return 0;
   }
}

int
habit_file_create(Habit *habit) {
   FILE *habit_file;
   char *filename = habit_make_filename(habit);

   habit_file = fopen(filename, "r");
   if (habit_file != NULL) {
      fclose(habit_file);
      free(filename);
      return 1;
   } else {
      habit_file = fopen(filename, "w");
      fprintf(habit_file, "%ld,%d,%d\n", 
             habit->stats.creation_timestamp, 
             habit->stats.completed_days,
             habit->stats.failed_days);
      fclose(habit_file);
      free(filename);
      return 0;
   }
}

// habit.days needs to be freed after it is no longer needed in the program.
int
habit_file_read(Habit *habit) {
   char *filename = habit_make_filename(habit);
   FILE *habit_file;
   if (!file_exists(filename)) {
      habit_file = fopen(filename, "r");
   } else {
      free(filename);
      return 1;
   }

   int day = 0;
   int mark = 0;
   fscanf(habit_file, "%ld,%d,%d\n",
          &habit->stats.creation_timestamp,
          &habit->stats.completed_days,
          &habit->stats.failed_days);
   while (fscanf(habit_file, "%d,%d\n", &day, &mark) != EOF) {
      habit_add_day(habit, day, mark);
   }

   fclose(habit_file);
   free(filename);
   return 0;
}

int
habit_file_write(Habit *habit, char *mode) {
   char *filename = habit_make_filename(habit);
   FILE *habit_file = fopen(filename, "r");
   if (habit_file == NULL) {
      fprintf(stderr, "file not found\n");
      free(filename);
      return 1;
   }
   fclose(habit_file);

   habit_file = fopen(filename, mode);
   if (!strcmp(mode, "w"))
      fprintf(habit_file, "%ld,%d,%d\n", 
             habit->stats.creation_timestamp, 
             habit->stats.completed_days,
             habit->stats.failed_days);
   if (habit->days_count != 0)
      for (int i = 0; i < habit->days_count; i++)
         if (habit->days[i].marked != DAY_UNMARKED)
            fprintf(habit_file, "%ld,%d\n", 
                    habit->days[i].timestamp, 
                    habit->days[i].marked);

   fclose(habit_file);
   free(filename);
   return 0;
}


// this function does not free() anything that it has allocated. 
// A programmer is responsible for free()ing habit->days array, after
// it is no longer needed in a program.
Day*
habit_add_day(Habit *habit, time_t timestamp, int mark) {
   if (habit->days_count == 0) {
      habit->days_count++;
      habit->days = calloc(1, sizeof(Day));
   }
   else {
      habit->days_count++;
      habit->days = realloc(habit->days, sizeof(Day) * habit->days_count);
   }

   habit->days[habit->days_count-1].marked = mark;
   habit->days[habit->days_count-1].timestamp = timestamp;

   return &habit->days[habit->days_count-1];
}

Day*
habit_add_today(Habit *habit, int mark) {
   return habit_add_day(habit, time(NULL), mark);
}

time_t
ymd_to_time_t(int year, int month, int day) {
   time_t timestamp;
   time(&timestamp);

   struct tm *timeinfo;
   timeinfo = localtime(&timestamp);
   timeinfo->tm_mday = day;
   timeinfo->tm_mon = month-1;
   timeinfo->tm_year = year-1900;

   time_t new_timestamp;
   new_timestamp = mktime(timeinfo);

   return new_timestamp;
}

int
date_compare(time_t ts1, time_t ts2) {
   struct tm tm1, tm2;
   localtime_r(&ts1, &tm1);
   localtime_r(&ts2, &tm2);

   if (tm1.tm_year < tm2.tm_year) return -1;
   else if (tm1.tm_year > tm2.tm_year) return 1;
   else if (tm1.tm_mon < tm2.tm_mon) return -1;
   else if (tm1.tm_mon > tm2.tm_mon) return 1;
   else if (tm1.tm_mday < tm2.tm_mday) return -1;
   else if (tm1.tm_mday > tm2.tm_mday) return 1;
   else return 0;
}

// this function doesn't free anything that it has malloc()ed
char*
habit_make_filename(Habit *habit) {
   char *path = habitpath(1);
   char *filename = malloc(strlen(path) + strlen(habit->name) + sizeof(char) * 5);
   strcpy(filename, path);
   strcat(filename, habit->name);
   strcat(filename, ".csv");
   free(path);
   return filename;
}

// this function doesn't free anything that it has malloc()ed
char*
default_habit_path_make() {
   char *habit_path = habitpath(1);
   char *path = malloc(strlen(habit_path) 
                       + strlen(default_habit_filename) 
                       + sizeof(char));
   strcpy(path, habit_path);
   strcat(path, default_habit_filename);
   free(habit_path);
   return path;
}

int
default_habit_write(Habit *habit) {
   char *default_path = default_habit_path_make();
   FILE *default_file = fopen(default_path, "w");
   if (default_file == NULL) return 1;
   fprintf(default_file, "%s\n", habit->name);
   fclose(default_file);
   free(default_path);
   return 0;
}

// this function doesn't free anything that it has malloc()ed
char*
default_habit_read() {
   char *default_path = default_habit_path_make();
   FILE *default_file;
   if (file_exists(default_path)) {
      free(default_path);
      return NULL;
   } else {
      default_file = fopen(default_path, "r");
   }

   char buffer[256];
   fgets(buffer, 256, default_file);

   char *name = malloc(strlen(buffer));
   for (int i = 0; i < strlen(buffer) - 1; i++) {
         name[i] = buffer[i]; 
   }
   name[strlen(buffer)-1] = '\0';


   fclose(default_file);
   free(default_path);
   return name;
}

void
day_sort(Day arr[], int len) {
   // this is insertion sort, very good for kind of arrays that are somewhat
   // presorted beforehand
   int j;
   Day temp;
   for (int i = 1; i < len; i++) {
      j = i;
      while(j && arr[j-1].timestamp > arr[j].timestamp) {
            temp = arr[j];
            arr[j] = arr[j-1];
            arr[j-1] = temp;
            j -= 1;
      }
   }
}

void
print_help() {
   printf(" === ht - CLI Habit Tracker === \n");
   printf("ht -h : print this message\n");
   printf("ht -a <name> : add new habit\n");
   printf("ht -r <name> : remove habit\n");
   printf("ht -l        : list habits\n");
   printf("ht -s <name> : select habit as a default one\n");
   printf("ht -s : unmark habit as default one (if it it was selected beforehand)\n");
   printf("ht : mark default habit as completed for today\n");
   printf("     (running this once again will unmark the habit)\n");
   printf("ht <YYYY.MM.DD> : mark default habit as completed for any day before today\n");
   printf("     (running this once again will unmark the habit)\n");
   printf("ht -H <habit> : mark any habit that isn't default\n");
   printf("ht -H <habit> <YYYY.MM.DD> : mark any day of the habit\n");
   printf("ht -c : display graph with a default habit\n");
   printf("ht -c <habit> : display graph with a selected habit\n");
   printf("      : custom width of the graph can be set with -w [1-39] argument\n");
   printf("ht -cl : display list of days of a default habit\n");
   printf("ht -cl <habit> : display list of days of a selected habit\n");
   printf("\nUse $%s to select habit path\n", habit_folder_path_env_var);
   printf("Set $%s to \"1\" to use nerd font symbols in the -c graph\n", nerd_font_var);
   printf("This help message is displayed when no default habit was selected\n");
   print_version();
}

Day*
day_exists(Habit *habit, time_t time) {
   struct tm ti1, ti2;
   for (int i = 0; i < habit->days_count; i++) {
      localtime_r(&habit->days[i].timestamp, &ti1);
      localtime_r(&time, &ti2);
      if (ti1.tm_year == ti2.tm_year &&
          ti1.tm_mon == ti2.tm_mon &&
          ti1.tm_mday == ti2.tm_mday) {
         return &habit->days[i];
      }
   }
   return NULL;
}

char*
habitpath(int trailing_slash) {
   char *path;
   char *envpath = getenv(habit_folder_path_env_var);
   if (envpath == NULL) {
      const char *homepath = getenv("HOME");
      path = malloc(strlen(default_habit_path) 
                    + strlen(homepath) 
                    + sizeof(char) * 1);
      strcpy(path, homepath);
      strcat(path, default_habit_path);
   } else {
      path = malloc(strlen(envpath) + sizeof(char) * 1);
      strcpy(path, envpath);
   }
   if (trailing_slash) {
      path = realloc(path, strlen(path) + sizeof(char) * 2);
      strcat(path, "/");
   }
   return path;
}

void 
switch_day_mark(Day *day) {
   switch(day->marked) {
      case DAY_UNMARKED: {
         day->marked = DAY_COMPLETE;
      } break;
      case DAY_COMPLETE: {
         day->marked = DAY_FAILED;
      } break;
      case DAY_FAILED: {
         day->marked = DAY_UNMARKED;
      } break;
   }
}

void
calc_stats(Habit *habit) {
   habit->stats.completed_days = 0;
   habit->stats.failed_days = 0;
   for (int i = 0; i < habit->days_count; i++) {
      if (habit->days[i].marked == DAY_COMPLETE)
         habit->stats.completed_days++;
      else if (habit->days[i].marked == DAY_FAILED)
         habit->stats.failed_days++;
   }
}

char*
day_mark_str(int day_mark) {
   switch(day_mark) {
      case DAY_COMPLETE: {
         return ANSI_GRN "DONE" ANSI_RESET;
      } break;
      case DAY_FAILED: {
         return ANSI_RED "fail" ANSI_RESET;
      } break;
      case DAY_UNMARKED: {
         return "deleted";
      } break;
      default: return NULL;
   }
}

void
print_version() {
   printf("ht: version 1.1.0dev\n");
}

void
graph_cell_print(int cell) {
   char *nerd_font = getenv(nerd_font_var);
   int c = (nerd_font != NULL) ? strcmp(nerd_font, "1") : 1;
   if (!c) {
      switch (cell) {
         case DAY_COMPLETE: printf(ANSI_GRN " " ANSI_RESET); break;
         case DAY_FAILED:   printf(ANSI_RED " " ANSI_RESET); break;
         case DAY_UNMARKED: printf(ANSI_RESET "  " ANSI_RESET); break;
         case GRAPH_BLANK_ODD:
            printf(ANSI_BLK " " ANSI_RESET); break;
         case GRAPH_BLANK_EVEN:
            printf(ANSI_BLK " " ANSI_RESET); break;
      }
   } else {
      switch (cell) {
         case DAY_COMPLETE: printf(ANSI_BGGRN "  " ANSI_RESET); break;
         case DAY_FAILED:   printf(ANSI_BGRED "  " ANSI_RESET); break;
         case DAY_UNMARKED: printf(ANSI_RESET "  " ANSI_RESET); break;
         case GRAPH_BLANK_ODD:
            printf(ANSI_BGIBLK "  " ANSI_RESET); break;
         case GRAPH_BLANK_EVEN:
            printf(ANSI_BGGRY "  " ANSI_RESET); break;
      }
   }
}

void
fillout_skipped_days(Habit *habit) {
   char *env_var = getenv(mark_prev_unmarked_days_as_failed_env_var);
   int cmp = (env_var != NULL) ? strcmp(env_var, "0") : 1;
   if (cmp) {
      time_t now = time(NULL);
      int d = habit->days_count-1;
      while (date_compare(now, habit->stats.creation_timestamp) >= 0) {
         if (date_compare(habit->days[d].timestamp, now) == 0) {
            if (d > 0) d--;
         } else {
            habit_add_day(habit, now, DAY_FAILED);
         }
         now -= 86400;
      }
      day_sort(habit->days, habit->days_count);
   }
}

void
habit_display_graph(Habit *habit, int is_default, int custom_width) {
   printf("Habit:" ANSI_YEL " %s" ANSI_RESET, habit->name);
   if (is_default) printf(" (default)");
   printf("\n");
   habit_file_read(habit);
   int is_custom = 0;
   int graph[GRAPH_XMAX][GRAPH_YMAX] = {0};
   int last_column = 0;
   int pattern = 0;
   int graph_fillout = -1;
   int dc = habit->days_count-1;

   time_t now = time(NULL);
   time_t graph_limit = now-86400*GRAPH_XMAX*GRAPH_YMAX;
   time_t t = time(NULL);
   struct tm *ti = localtime(&t);
   int offset = (ti->tm_wday == 0) ? 7 : ti->tm_wday;
   offset = 7 - offset;

   typedef struct {
      int i;
      const char *name;
   } Month;
   Month *months = NULL;
   int months_count = 0;

   int compare = 0;
   for (int y = GRAPH_YMAX-1; y >= 0; y--) {
      for (int x = GRAPH_XMAX-1; x >= 0; x--) {
         if (y == GRAPH_YMAX-1 && offset != 0 && offset != 7) {
            offset--;
            continue;
         } else {
            if (dc >= 0) {
               compare = date_compare(habit->days[dc].timestamp, t);
               if (compare == 0) {
                  if (habit->days[dc].marked == DAY_COMPLETE
                     || habit->days[dc].marked == DAY_FAILED) {
                     graph[x][y] = habit->days[dc].marked;
                     if (!is_custom && 
                        date_compare(graph_limit, habit->days[dc].timestamp)
                        == 1) {
                        dc = -1;
                        graph_fillout = 10;
                     } else if (!is_custom && dc == 0) {
                        dc = -1; 
                        graph_fillout = 10;
                     }
                     if (dc >= 0) dc--;
                  }
               }
            } else {
               compare = 1;
            }
            if (compare && pattern % 2 == 0) {
               graph[x][y] = GRAPH_BLANK_EVEN;
            } else if (compare && pattern % 2 == 1) {
               graph[x][y] = GRAPH_BLANK_ODD;
            }

            ti = localtime(&t);
            if (ti->tm_mday == 1) {
               if (months_count == 0) {
                  months_count = 1;
                  months = malloc(sizeof(Month) * months_count);
               } else {
                  months_count++;
                  months = realloc(months, sizeof(Month) * months_count);
               }
               switch (ti->tm_mon) {
                  case  0: months[months_count-1].name = "Jan"; break;
                  case  1: months[months_count-1].name = "Feb"; break;
                  case  2: months[months_count-1].name = "Mar"; break;
                  case  3: months[months_count-1].name = "Apr"; break;
                  case  4: months[months_count-1].name = "May"; break;
                  case  5: months[months_count-1].name = "Jun"; break;
                  case  6: months[months_count-1].name = "Jul"; break;
                  case  7: months[months_count-1].name = "Aug"; break;
                  case  8: months[months_count-1].name = "Sep"; break;
                  case  9: months[months_count-1].name = "Oct"; break;
                  case 10: months[months_count-1].name = "Nov"; break;
                  case 11: months[months_count-1].name = "Dec"; break;
               }
               months[months_count-1].i = y;
            }
            t -= 86400;
            pattern++;
         }
      }

      if (graph_fillout > 0) {
         --graph_fillout;
         last_column = y;
      } else if (graph_fillout == 0) {
         break;
      } else {
         if (is_custom)
            if (y <= GRAPH_YMAX-custom_width) {
               last_column = y;
               break;
            }
      }
   }

   const char* sep = "  ";
   const char* half_sep = " ";

   int i = 0;
   if (months_count > 0) {
      i = months_count-1;
      for (int k = 0; k < months_count; k++)
         if (months[i].i < last_column && i > 0)
            i--;
      int nextishalf = 0;
      printf("   ");
      for (int y = last_column; y < GRAPH_YMAX; y++) {
         if (months[i].i == y) {
            printf("%s", months[i].name);
            if (i != 0) i--;
            nextishalf = 1;
         } else if (nextishalf == 1) {
            printf("%s", half_sep);
            nextishalf = 0;
         } else printf("%s", sep);
      }
   }
   printf("\n");
   for (int x = 0; x < GRAPH_XMAX; x++) {
      switch (x) {
         case 0: printf("Mon"); break;
         case 2: printf("Wed"); break;
         case 4: printf("Fri"); break;
         case 6: printf("Sun"); break;
         default: printf("   ");
      }
      for (int y = last_column; y < GRAPH_YMAX; y++) {
         graph_cell_print(graph[x][y]);
      }
      switch (x) {
         case 0: printf("Mon"); break;
         case 2: printf("Wed"); break;
         case 4: printf("Fri"); break;
         case 6: printf("Sun"); break;
         default: printf("   ");
      }
      printf("\n");
   }
   if (months != NULL) free(months);
   if (habit->stats.completed_days == 0
      && habit->stats.failed_days == 0)
      calc_stats(habit);
   printf("Days total: %d;" ANSI_GRN " completed: %d;" ANSI_RED " failed %d;\n", 
          habit->days_count,
          habit->stats.completed_days,
          habit->stats.failed_days);
   ti = localtime(&habit->stats.creation_timestamp);
   printf(ANSI_RESET "Created at: %d.%02d.%02d\n", 
          ti->tm_year + 1900,
          ti->tm_mon + 1,
          ti->tm_mday);
}

void
habit_display_as_list(Habit *habit, int is_default) {
   printf("Habit:" ANSI_YEL " %s" ANSI_RESET, habit->name);
   if (is_default) printf(" (default)");
   printf("\n");
   habit_file_read(habit);
   for (int i = 0; i < habit->days_count; i++) {
      struct tm *timeinfo = localtime(&habit->days[i].timestamp);
      switch(habit->days[i].marked) {
         case DAY_COMPLETE: {
            printf(ANSI_GRN);
         } break;
         case DAY_FAILED: {
            printf(ANSI_RED);
         } break;
         case DAY_UNMARKED: {
            printf(ANSI_BLK);
         } break;
      }
      printf("day: %d.%02d.%02d %s" ANSI_RESET "\n",
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             day_mark_str(habit->days[i].marked));
   }
   if (habit->stats.completed_days == 0
      && habit->stats.failed_days == 0)
      calc_stats(habit);
   printf("Days total: %d;" ANSI_GRN " completed: %d;" ANSI_RED " failed %d;\n", 
          habit->days_count,
          habit->stats.completed_days,
          habit->stats.failed_days);
   struct tm *ti = localtime(&habit->stats.creation_timestamp);
   printf(ANSI_RESET "Created at: %d.%02d.%02d\n", 
          ti->tm_year + 1900,
          ti->tm_mon + 1,
          ti->tm_mday);
   free(habit->days);
}
