#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

// TODO: add multiple days in one command
// TODO: improve cli handling
// TODO: delete days
// TODO: GitHub-like calender of habits
// TODO: support for multiple date formats (YYYY.MM.DD/DD.MM.YYYY/MM.DD.YYYY)
//       by the means of env variable
// TODO: as of now, it is very difficult to select a habit 
//       (maybe use `fzf' if it is present in the system?)

// A Habit Tracker
//
// Each habit is a separate CSV file with a name of habit
// as the name of the file. Structure of CSV:
//   Day,Marked

extern int errno;

#define ANSI_BGRED "\e[41m"
#define ANSI_BGGRN "\e[42m"
#define ANSI_BGIBLK "\e[0;100m"
#define ANSI_BGGRY "\e[40m"
#define ANSI_RESET "\e[0m"
#define GRAPH_BLANK_EVEN 2
#define GRAPH_BLANK_ODD 3
#define GRAPH_XMAX 7
#define GRAPH_YMAX 52


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

const char *habit_create_arg = "-a";
const char *habit_remove_arg = "-r";
const char *habit_list_arg = "-l";
const char *habit_select_default_arg = "-s";
const char *habit_folder_select_arg = "-f";
const char *habit_display_arg = "-c";
const char *habit_add_day_arg = "-d";
const char *habit_non_default_arg = "-H";
const char *print_help_arg = "-h";

int habit_file_create(Habit *habit);
int habit_file_write(Habit *habit, char *mode);
int habit_file_read(Habit *habit);
Day *habit_add_day(Habit *habit, time_t time, int mark);
Day *habit_add_today(Habit *habit, int mark);
int default_habit_write(Habit *habit);
char *default_habit_read();
time_t ymd_to_time_t(int year, int month, int day);
int date_compare(time_t ts1, time_t ts2);
char *habit_make_filename(Habit *habit);
void print_help();
Day *day_exists(Habit *habit, time_t time);
void day_sort(Day arr[], int len);
char *habitpath(int trailing_slash);
char *default_habit_path_make();
void switch_day_mark(Day *day);
void calc_stats(Habit *habit);
char *day_mark_str(int day_mark);

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

   char *habit_path_tr = habitpath(1);
   Habit habit = {0};
   if (argc <= 1) {
      FILE *default_file = fopen(default_habit_path_make(), "r");
      if (default_file == NULL) {
         print_help();
         return 0;
      }
      fclose(default_file);

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
      if (argc >= 3) {
         habit.name = argv[2];
         if (habit_file_read(&habit)) {
            fprintf(stderr, "%s does not exist\n", habit.name);
            return 1;
         }
         char *filename = habit_make_filename(&habit);
         char choice = 0;

         printf("Are you sure you want to delete this habit?: [y/N] ");
         scanf("%c", &choice);
         if (choice == 'y' || choice == 'Y') {
            remove(filename);
            printf("%s has been deleted successfully\n", habit.name);
         } else return 0;
      } else {
         fprintf(stderr, "No habit name was provided\n");
         return 1;
      }
   } else if (!strcmp(argv[1], habit_select_default_arg)) {
      if (argc >= 3) {
         habit.name = argv[2];
         if (habit_file_read(&habit)) {
            fprintf(stderr, "Habit doesn't exist\n");
            return 1;
         }
         if (default_habit_write(&habit)) {
            fprintf(stderr, "Unable to write a default habit\n");
            return 1;
         }
         printf("%s has been selected as a default habit\n", habit.name);
      } else if (default_habit_read() == NULL) {
         printf("Provide a habit name to set it as a default one\n");
         return 1;
      } else if (default_habit_read() != NULL) {
         remove(default_habit_path_make());
         printf("Default habit has been unselected\n");
      }
   } else if (!strcmp(argv[1], habit_list_arg)) {
      struct dirent *de;
      DIR *dr = opendir(habit_path);
      if (dr == NULL) {
         perror("ERROR");
      }
      while ((de = readdir(dr)) != NULL) 
         if (strcmp(de->d_name, "..") 
             && strcmp(de->d_name, ".")
             && strcmp(de->d_name, "default")) {
            habit.name = calloc(strlen(de->d_name)-3, sizeof(char));
            habit.days_count = 0;
            for (int i = 0; i < strlen(de->d_name)-4; i++) {
               habit.name[i] = de->d_name[i];
            }
            habit.name[strlen(de->d_name)] = '\0';
            habit_file_read(&habit);
            struct tm *ti = localtime(&habit.stats.creation_timestamp);
            printf("%s", habit.name);
            if (habit.days_count > 0) {
               printf(" (TODAY: %s)\n", day_mark_str(habit.days[habit.days_count-1].marked));
               printf("   Days completed: %d; failed: %d; created %d.%02d.%02d\n", 
                      habit.stats.completed_days,
                      habit.stats.failed_days,
                      ti->tm_year + 1900,
                      ti->tm_mon + 1,
                      ti->tm_mday);
            }
            else printf("\n");
         }
   } else if (!strcmp(argv[1], habit_display_arg)) {
      if (argc == 2) habit.name = default_habit_read();
      else habit.name = argv[2];

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
      printf("Habit name: %s\n", habit.name);
      if (habit.days_count != 0) {
         int graph[GRAPH_XMAX][GRAPH_YMAX] = {0};

         time_t t = time(NULL);
         struct tm *ti = localtime(&t);
         int offset = (ti->tm_wday == 0) ? 7 : ti->tm_wday;
         offset++;

         int last_column = 40;
         int pattern = 0;
         typedef struct {
            int i;
            const char *name;
         } Month;
         Month *months;
         int months_count = 0;

         for (int y = GRAPH_YMAX-1; y >= 0; y--) {
            for (int x = GRAPH_XMAX-1; x >= 0; x--) {
               if (y == GRAPH_YMAX-1 && offset != 0) {
                  offset--;
                  continue;
               } else {
                  Day *day = day_exists(&habit, t);
                  if (day) {
                     if (day->marked == DAY_COMPLETE || day->marked == DAY_FAILED)
                        graph[x][y] = day->marked;
                  } else if (pattern % 2 == 0) {
                     graph[x][y] = GRAPH_BLANK_EVEN;
                  } else if (pattern % 2 == 1) {
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
            if (date_compare(t, habit.days[0].timestamp) == -1 
                && y <= last_column) {
               last_column = y;
               break;
            }
         }

         const char* sep = "  ";
         const char* half_sep = " ";
         int i = months_count-1;
         int nextishalf = 0;
         printf("   ");
         for (int y = last_column; y < GRAPH_YMAX; y++) {
            if (months[i].i == y) {
               printf("%s", months[i].name);
               i--;
               nextishalf = 1;
            } else if (nextishalf == 1) {
               printf("%s", half_sep);
               nextishalf = 0;
            } else printf("%s", sep);
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
               switch(graph[x][y]) {
                  case DAY_COMPLETE: printf(ANSI_BGGRN "  " ANSI_RESET); break;
                  case DAY_FAILED:   printf(ANSI_BGRED "  " ANSI_RESET); break;
                  case DAY_UNMARKED: printf(ANSI_RESET "  " ANSI_RESET); break;
                  case GRAPH_BLANK_ODD: 
                     printf(ANSI_BGIBLK "  " ANSI_RESET); break;
                  case GRAPH_BLANK_EVEN: 
                     printf(ANSI_BGGRY "  " ANSI_RESET); break;
               }
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
         free(months);
      }


      /*int a[7][*/

      /*for (int i = 0; i < habit.days_count; i++) {*/
      /*   struct tm *timeinfo = localtime(&habit.days[i].timestamp);*/
      /*   printf("day: %d.%02d.%02d ",*/
      /*          timeinfo->tm_year + 1900,*/
      /*          timeinfo->tm_mon + 1,*/
      /*          timeinfo->tm_mday);*/
      /*   switch(habit.days[i].marked) {*/
      /*      case DAY_UNMARKED: {*/
      /*         printf("unmarked\n"); */
      /*      } break;*/
      /*      case DAY_COMPLETE: {*/
      /*         printf("DONE\n"); */
      /*      } break;*/
      /*      case DAY_FAILED: {*/
      /*         printf("fail\n"); */
      /*      } break;*/
      /*   }*/
      /*}*/

      if (habit.stats.completed_days == 0
         && habit.stats.failed_days == 0)
            calc_stats(&habit);
      printf("Total days: %d\n", habit.days_count);
      printf("Completed days: %d\n", habit.stats.completed_days);
      printf("Failed days: %d\n", habit.stats.failed_days);
      struct tm *ti = localtime(&habit.stats.creation_timestamp);
      printf("Creation date: %d.%02d.%02d\n", 
             ti->tm_year + 1900,
             ti->tm_mon + 1,
             ti->tm_mday);
   } else {
      // TODO: maybe add some sort of way to check if this piece of code
      // has actually successfully worked out
      time_t new_day_timestamp;
      char *day_arg = NULL;
      int year, month, day;

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
         day_arg = argv[1];
      }
      if (day_arg != NULL) {
         sscanf(day_arg, "%d.%d.%d", &year,&month,&day);
         new_day_timestamp = ymd_to_time_t(year, month, day);
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
                      year, month, day);
               return 1;
            }

            if (habit.days[habit.days_count-2].timestamp >
                habit.days[habit.days_count-1].timestamp)
               day_sort(habit.days, habit.days_count);
         }
      }

      calc_stats(&habit);
      habit_file_write(&habit, "w");
   }

   free(habit.days);
   free(habit_path);
   free(habit_path_tr);
   return 0;
}

int
habit_file_create(Habit *habit) {
   FILE *habit_file;
   char *filename = habit_make_filename(habit);

   habit_file = fopen(filename, "r");
   if (habit_file != NULL) {
      fclose(habit_file);
      return 1;
   } else {
      habit_file = fopen(filename, "w");
      fprintf(habit_file, "%ld,%d,%d\n", 
             habit->stats.creation_timestamp, 
             habit->stats.completed_days,
             habit->stats.failed_days);
      fclose(habit_file);
      return 0;
   }
}

int
habit_file_delete(Habit *habit) {
   return 0;
}

int
habit_file_read(Habit *habit) {
   char *filename = habit_make_filename(habit);
   FILE *habit_file = fopen(filename, "r");
   if (habit_file == NULL) {
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
   return 0;
}

int
habit_file_write(Habit *habit, char *mode) {
   char *filename = habit_make_filename(habit);
   FILE *habit_file = fopen(filename, "r");
   if (habit_file == NULL) {
      fprintf(stderr, "file not found\n");
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
         fprintf(habit_file, "%ld,%d\n", 
                 habit->days[i].timestamp, 
                 habit->days[i].marked);

   fclose(habit_file);
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

char*
default_habit_path_make() {
   char *habit_path = habitpath(1);
   char *path = malloc(strlen(habit_path) 
                       + strlen(default_habit_filename) 
                       + sizeof(char));
   strcpy(path, habit_path);
   strcat(path, default_habit_filename);
   return path;
}

int
default_habit_write(Habit *habit) {
   // TODO: add error checking
   FILE *default_file = fopen(default_habit_path_make(), "w");
   if (default_file == NULL) return 1;
   fprintf(default_file, "%s\n", habit->name);
   fclose(default_file);
   return 0;
}

char*
default_habit_read() {
   FILE *default_file = fopen(default_habit_path_make(), "r");
   if (default_file == NULL) return NULL;

   char buffer[256];
   fgets(buffer, 256, default_file);

   char *name = malloc(strlen(buffer));
   for (int i = 0; i < strlen(buffer) - 1; i++) {
         name[i] = buffer[i]; 
   }
   name[strlen(buffer)] = '\0';


   fclose(default_file);
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
   printf("!TBD! ht -r <name> : remove habit\n");
   printf("!TBD! ht -l        : list habits\n");
   printf("ht -s <name> : select habit as a default one\n");
   printf("ht -s : unmark habit as default one (if it it was selected beforehand\n");
   printf("ht : mark default habit as completed for today\n");
   printf("     (running this once again will unmark the habit)\n");
   printf("ht <YYYY.MM.DD> : mark default habit as completed for any day before today\n");
   printf("     (running this once again will unmark the habit)\n");
   printf("ht -H <habit> : mark any habit that isn't default\n");
   printf("ht -H <habit> <YYYY.MM.DD> : mark any day of the habit\n");
   printf("ht -c : display calender with a default habit\n");
   printf("ht -c <habit> : display calender with a selected habit\n");
   printf("\nUse $%s to select habit path\n", habit_folder_path_env_var);
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

// TODO: this is never free()ed in this code. shameful.
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
      path = realloc(path, strlen(path) + sizeof(char));
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
         return "unmarked";
      } break;
      default: return NULL;
   }
}
