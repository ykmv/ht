#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

// TODO: If there are no habits `ht -l` will segfault if `ht` was compiled
//       with `tcc`.
// TODO: support for multiple date formats (YYYY.MM.DD/DD.MM.YYYY/MM.DD.YYYY)
//       by the means of env variable
// TODO: as of now, it is very difficult to select a habit 
//       (maybe use `fzf' if it is present in the system?)
// TODO: Add a check for ANSI colours support
// TODO: add -y to the -c argument to allow to see a specific year

// A Habit Tracker
//
// Each habit is a separate CSV file with a name of habit
// as the name of the file. Structure of CSV:
//   Day,Marked

extern int errno;

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
} Habit;

const char *default_habit_filename = "default";
const char *habit_folder_path_env_var = "HTDIR";
const char *default_habit_path = "/.local/share/ht";
const char *date_format_env_var = "HTFORMAT";
const char *nerd_font_var = "HTNERD";

const char *habit_create_arg = "-a";
const char *habit_remove_arg = "-r";
const char *habit_list_arg = "-l";
const char *habit_select_default_arg = "-s";
const char *habit_folder_select_arg = "-f";
const char *habit_display_arg = "-c";
const char *habit_display_as_list_arg = "-cl";
const char *habit_add_day_arg = "-d";
const char *habit_non_default_arg = "-H";
const char *print_help_arg = "-h";
const char *version_arg = "-v";

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

int
main(int argc, char *argv[]) {
   char *habit_path = habitpath(0);

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

   Habit habit = {0};
   habit.name = "";
   if (argc <= 1) {
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

      calc_stats(&habit);
      habit_file_write(&habit, "w");
      free(habit.name);
      free(default_path);
   } else if (!strcmp(argv[1], version_arg)) {
      print_version();
   } else if (!strcmp(argv[1], print_help_arg)) {
      print_help();
   } else if (!strcmp(argv[1], habit_create_arg)) {
      if (argc < 3) {
         fprintf(stderr, "No habit name provided\n");
         return 1;
      }
      habit.name = argv[2];
      habit.stats.creation_timestamp = time(NULL);

      if (habit_file_create(&habit)) {
         fprintf(stderr, "Habit %s already exists\n", habit.name);
         return 1;
      } else {
         printf("Habit %s created\n", habit.name);
      }
   } else if (!strcmp(argv[1], habit_remove_arg)) {
      char *filename = "";
      if (argc >= 3) {
         habit.name = argv[2];
         filename = habit_make_filename(&habit);
         if (file_exists(filename)) {
            fprintf(stderr, "%s does not exist\n", habit.name);
            free(filename);
            return 1;
         }
         char choice = 0;

         printf("Are you sure you want to delete this habit?: [y/N] ");
         scanf("%c", &choice);
         if (choice == 'y' || choice == 'Y') {
            remove(filename);
            printf("%s has been deleted successfully\n", habit.name);
            char *default_habit_name = default_habit_read();
            if (default_habit_name != NULL) {
               char *default_habit_path = default_habit_path_make();
               remove(default_habit_path);
               printf("%s was default, default habit is no longer set\n", habit.name);
            }
         }
      } else {
         fprintf(stderr, "No habit name was provided\n");
         return 1;
      }
      free(filename);
   } else if (!strcmp(argv[1], habit_select_default_arg)) {
      char* default_path = "";
      if (argc >= 3) {
         habit.name = argv[2];
         char *habit_path = habit_make_filename(&habit);
         if (file_exists(habit_path)) {
            fprintf(stderr, "Habit doesn't exist\n");
            free(habit_path);
            return 1;
         }
         if (default_habit_write(&habit)) {
            fprintf(stderr, "Unable to write a default habit\n");
            free(habit_path);
            return 1;
         }
         printf("%s has been selected as a default habit\n", habit.name);
         free(habit_path);
      } else if ((default_path = default_habit_read()) == NULL) {
         fprintf(stderr, "Provide a habit name to set it as a default one\n");
         return 1;
      } else if (default_path != NULL) {
         char *default_habit_path_file = default_habit_path_make();
         remove(default_habit_path_file);
         free(default_habit_path_file);
         free(default_path);
         printf("Default habit has been unselected\n");
      }
   } else if (!strcmp(argv[1], habit_list_arg)) {
      struct dirent *de;
      char *habit_path = habitpath(0);
      DIR *dr = opendir(habit_path);
      if (dr == NULL) {
         perror("ERROR");
         free(habit_path);
         return 1;
      }

      Habit *habits;
      int habit_count = 0;
      while ((de = readdir(dr)) != NULL)
         if (strcmp(de->d_name, "..")
             && strcmp(de->d_name, ".")
             && strcmp(de->d_name, "default")) {
            if (habit_count == 0) {
               habit_count = 1;
               habits = calloc(1, sizeof(Habit));
            } else {
               habit_count++;
               habits = realloc(habits, habit_count * sizeof(Habit));
            }
            habits[habit_count-1].name = calloc(strlen(de->d_name), sizeof(char));
            habits[habit_count-1].days_count = 0;
            habits[habit_count-1].days = NULL;
            habits[habit_count-1].stats.completed_days = 0;
            habits[habit_count-1].stats.failed_days = 0;
            habits[habit_count-1].stats.creation_timestamp = 0;

            for (int i = 0; i < strlen(de->d_name)-4; i++)
               habits[habit_count-1].name[i] = de->d_name[i];
            habits[habit_count-1].name[strlen(de->d_name)-1] = '\0';
            habit_file_read(&habits[habit_count-1]);
            printf("%s", habits[habit_count-1].name);
            if (habits[habit_count-1].days_count > 0) {
               if (!date_compare(
                     habits[habit_count-1].
                        days[habits[habit_count-1].days_count-1].timestamp, 
                     time(NULL)))
                  printf(" (TODAY: %s)",
                         day_mark_str(
                           habits[habit_count-1].
                           days[habits[habit_count-1].days_count-1].marked));
               printf("\n");
               printf("   Days "
                      ANSI_GRN "completed: %d; "
                      ANSI_RED "failed: %d; ",
                         habits[habit_count-1].stats.completed_days,
                         habits[habit_count-1].stats.failed_days);
            } else printf("\n   ");
            struct tm *ti = localtime(
               &habits[habit_count-1].stats.creation_timestamp);
            printf(ANSI_RESET "created %d.%02d.%02d\n", 
                   ti->tm_year + 1900,
                   ti->tm_mon + 1,
                   ti->tm_mday);
            free(habits[habit_count-1].name);
            free(habits[habit_count-1].days);
         }
         free(habits);
         free(habit_path);
         closedir(dr);
         return 0;
   } else if (!strcmp(argv[1], habit_display_arg) 
      || !strcmp(argv[1], habit_display_as_list_arg)) {
      int is_default = 0;
      int is_custom = 0;
      int custom_width = 0;
      int arg_offset = 0;
      if (argc == 2) {
         is_default = 1;
         habit.name = default_habit_read();
      } else if (!strcmp(argv[2], "-w")) {
         is_default = 1;
         if (argc == 3) {
            fprintf(stderr, "Please provide a width\n");
            return 1;
         } else {
            is_custom = 1;
            custom_width = atoi(argv[3]);
         }
         habit.name = default_habit_read();
      } else {
         if (argc >= 4) {
            if (!strcmp(argv[3], "-w")) {
               if (argc == 4) { 
                  fprintf(stderr, "Please provide a width\n");
               } else {
                  is_custom = 1;
                  custom_width = atoi(argv[4]);
               }
            }
         }
         habit.name = argv[2];
      }

      if (habit.name == NULL && argc == 2) {
         fprintf(stderr,
            "No default habit was selected and no other habit was provided\n");
         return 1;
      } else if (habit.name == NULL) {
         fprintf(stderr, "No habit name was provided\n");
         return 1;
      }


      if (habit_file_read(&habit)) {
         fprintf(stderr, "This habit doesn't exist\n");
         return 1;
      }
      printf("Habit:" ANSI_YEL " %s" ANSI_RESET, habit.name);
      if (is_default) printf(" (default)");
      printf("\n");
      if (habit.days_count != 0) {
         if (!strcmp(argv[1], habit_display_as_list_arg)) {
            for (int i = 0; i < habit.days_count; i++) {
               struct tm *timeinfo = localtime(&habit.days[i].timestamp);
               switch(habit.days[i].marked) {
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
                  day_mark_str(habit.days[i].marked));
            }
         } else {
            int graph[GRAPH_XMAX][GRAPH_YMAX] = {0};
            int last_column = 0;
            int pattern = 0;
            int graph_fillout = -1;
            int dc = habit.days_count-1;

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
                        compare = date_compare(habit.days[dc].timestamp, t);
                        if (compare == 0) {
                           if (habit.days[dc].marked == DAY_COMPLETE
                              || habit.days[dc].marked == DAY_FAILED) {
                              graph[x][y] = habit.days[dc].marked;
                              if (!is_custom && 
                                 date_compare(graph_limit, habit.days[dc].timestamp)
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
         }
      }
      if (habit.stats.completed_days == 0
         && habit.stats.failed_days == 0)
            calc_stats(&habit);
      printf("Days total: %d;" ANSI_GRN " completed: %d;" ANSI_RED " failed %d;\n", 
             habit.days_count,
             habit.stats.completed_days,
             habit.stats.failed_days);
      struct tm *ti = localtime(&habit.stats.creation_timestamp);
      printf(ANSI_RESET "Created at: %d.%02d.%02d\n", 
             ti->tm_year + 1900,
             ti->tm_mon + 1,
             ti->tm_mday);
      if (is_default) free(habit.name);
   } else if (!strcmp(argv[1], habit_non_default_arg) || argv[1][0] != '-') {
      time_t new_day_timestamp;
      char *day_arg = NULL;
      int year, month, day;
      time_t time_now = time(NULL);
      struct tm *now = localtime(&time_now);
      int is_default = 0;

      if (!strcmp(argv[1], habit_non_default_arg)) {
         if (argc > 2)
            habit.name = argv[2]; 
         else {
            fprintf(stderr, "No habit name was provided\n");
            return 1;
         }
         if (argc > 3) day_arg = argv[3];
      } else {
         habit.name = default_habit_read();
         is_default = 1;
         day_arg = argv[1];
      }
      if (day_arg != NULL) {
         if (sscanf(day_arg, "%d.%d.%d", &year,&month,&day) == 3
             && year > 1
             && month >= 1 && month <= 12
             && day >= 1 && day <= 31) {
            new_day_timestamp = ymd_to_time_t(year, month, day);
         } else if (sscanf(day_arg, "%d.%d", &month, &day) == 2
                    && month >= 1 && month <= 12
                    && day >= 1 && day <= 31) {
            new_day_timestamp = ymd_to_time_t(now->tm_year+1900, month, day);
         } else if (sscanf(day_arg, "%d", &day) == 1 && day >= 1 && day <= 31) {
            new_day_timestamp = ymd_to_time_t(
               now->tm_year+1900, now->tm_mon + 1, day);
         } else {
            fprintf(stderr, "The provided date \"%s\" cannot be parsed\n", day_arg);
            free(habit.days);
            if (is_default) free(habit.name);
            return 1;
         }
      } else new_day_timestamp = time(NULL);

      habit_file_read(&habit);
      struct tm *ti = localtime(&new_day_timestamp);
      if (habit.days_count == 0) { 
         habit_add_day(&habit, new_day_timestamp, DAY_COMPLETE);
         printf("%d.%02d.%02d: %s\n", 
                ti->tm_year + 1900,
                ti->tm_mon + 1,
                ti->tm_mday,
                day_mark_str(DAY_COMPLETE));
      }
      else {
         Day *existing_day = day_exists(&habit, new_day_timestamp);
         if (existing_day != NULL) {
            int temp = existing_day->marked;
            switch_day_mark(existing_day);
            printf("%d.%02d.%02d: %s -> %s\n", 
                   ti->tm_year + 1900,
                   ti->tm_mon + 1,
                   ti->tm_mday,
                   day_mark_str(temp),
                   day_mark_str(existing_day->marked));
         } else {
            if (date_compare(time(NULL), new_day_timestamp) != -1) {
               habit_add_day(&habit, new_day_timestamp, DAY_COMPLETE);
               printf("%d.%02d.%02d: %s\n", 
                      ti->tm_year + 1900,
                      ti->tm_mon + 1,
                      ti->tm_mday,
                  day_mark_str(DAY_COMPLETE));
            } else {
               printf("The provided date %d.%02d.%02d is bigger than today\n",
                      ti->tm_year + 1900,
                      ti->tm_mon + 1,
                      ti->tm_mday);
               free(habit.days);
               return 1;
            }

            if (habit.days[habit.days_count-2].timestamp >
                habit.days[habit.days_count-1].timestamp)
               day_sort(habit.days, habit.days_count);
         }
      }

      calc_stats(&habit);
      habit_file_write(&habit, "w");
      if (is_default) free(habit.name);
   } else {
      fprintf(stderr, "Wrong argument (use \"ht -h\" to get help for commands)\n");
   }

   free(habit.days);
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
      path = malloc(strlen(default_habit_path) + strlen(homepath) + sizeof(char) * 1);
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
         return "DONE";
      } break;
      case DAY_FAILED: {
         return "fail";
      } break;
      case DAY_UNMARKED: {
         return "deleted";
      } break;
      default: return NULL;
   }
}

void
print_version() {
   printf("ht: version 1.0.0\n");
}

void
graph_cell_print(int cell) {
   char *nerd_font = getenv(nerd_font_var);
   int c = (nerd_font != NULL) ? strcmp(nerd_font, "1") : 1;
   if (!c) {
      switch (cell) {
         case DAY_COMPLETE: printf(ANSI_GRN " " ANSI_RESET); break;
         case DAY_FAILED:   printf(ANSI_RED " " ANSI_RESET); break;
         case DAY_UNMARKED: printf(ANSI_RESET " " ANSI_RESET); break;
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
