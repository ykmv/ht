#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

// A Habit Tracker
//
// Each habit is a separate CSV file with a name of habit
// as the name of the file. Structure of CSV:
//   Day,Marked

extern int errno;

#define DAY_UNMARKED 0
#define DAY_COMPLETE 1
#define DAY_FAILED -1
typedef struct {
   time_t timestamp;
   signed int marked;
} Day;


typedef struct {
   char *name;
   Day *days;
   int days_count;
} Habit;

const char *default_habit_filename = "default";
const char *habit_folder_path_env_var = "HTDIR";
const char *default_habit_path = "/.local/share/ht";
char *habit_path;


char *habit_create_arg = "-a";
char *habit_remove_arg = "-r";
char *habit_list_arg = "-l";
char *habit_select_default_arg = "-s";
char *habit_folder_select_arg = "-f";
char *habit_display_arg = "-c";
char *habit_add_day_arg = "-d";

int habit_file_create(Habit *habit);
int habit_file_write(Habit *habit, char *mode);
int habit_file_read(Habit *habit);
int habit_mark_day(Day *day, signed int status);
Day *habit_add_day(Habit *habit, time_t time, int mark);
Day *habit_add_today(Habit *habit, int mark);
int default_habit_write(Habit *habit);
char* default_habit_read();
time_t ymd_to_time_t(int year, int month, int day);
int date_compare(time_t ts1, time_t ts2);
char* habit_make_filename(Habit *habit);
void print_help();
Day* day_exists(Habit *habit, time_t time);
void day_sort(Day arr[], int len);
char *habitpath(int trailing_slash);
char *default_habit_path_make();
void switch_day_mark(Day *day);


// TODO: - Check if the provided date is bigger than today in -d

int
main(int argc, char *argv[]) {
   char *habit_path = habitpath(0);

   struct stat st = {0};
   int err = stat(habit_path, &st);
   if (err == -1) {
      mkdir(habit_path, 0700);
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

      if (habit.days_count == 0) {
         habit_add_today(&habit, DAY_COMPLETE); 
      }
      else if (date_compare(
                  habit.days[habit.days_count-1].timestamp, 
                  time(NULL)) == 0) {
         switch_day_mark(&habit.days[habit.days_count-1]);
      } else habit_add_today(&habit, DAY_COMPLETE); 

      habit_file_write(&habit, "w");
   } else if (!strcmp(argv[1], habit_create_arg)) {
      habit.name = argv[2];

      // TODO: look at the <errno.h> library
      int err = habit_file_create(&habit);
      if (err) {
         fprintf(stderr, "habit already exists\n");
         return 1;
      } else {
         printf("Habit created\n");
      }
   } else if (!strcmp(argv[1], habit_select_default_arg)) {
      habit.name = argv[2];
      int err = habit_file_read(&habit);
      if (err) {
         fprintf(stderr, "Habit doesn't exist\n");
         return 1;
      }

      err = default_habit_write(&habit);
      if (err) {
         fprintf(stderr, "Unable to write a default habit\n");
         return 1;
      }
   } else if (!strcmp(argv[1], habit_list_arg)) {
      printf("habit_list_arg\n");
   } else if (!strcmp(argv[1], habit_display_arg)) {
      // TODO: maybe separate habit stats into a different function?
      int complete_days = 0;
      int failed_days = 0;

      if (argc == 2) habit.name = default_habit_read();
      else habit.name = argv[2];

      habit_file_read(&habit);
      printf("Habit name: %s\n", habit.name);

      for (int i = 0; i < habit.days_count; i++) {
         if (habit.days[i].marked == DAY_COMPLETE)
            complete_days++;
         else if (habit.days[i].marked == DAY_FAILED)
            failed_days++;
      }

      for (int i = 0; i < habit.days_count; i++) {
         struct tm *timeinfo = localtime(&habit.days[i].timestamp);
         printf("day: %d.%02d.%02d ",
                timeinfo->tm_year + 1900,
                timeinfo->tm_mon + 1,
                timeinfo->tm_mday);
         switch(habit.days[i].marked) {
            case DAY_UNMARKED: {
               printf("unmarked\n"); 
            } break;
            case DAY_COMPLETE: {
               printf("DONE\n"); 
            } break;
            case DAY_FAILED: {
               printf("fail\n"); 
            } break;
         }
      }
      printf("Completed days: %d\n", complete_days);
      printf("Failed days: %d\n", failed_days);
   } else if (!strcmp(argv[1], habit_add_day_arg)) {
      // TODO: maybe add some sort of way to check if this piece of code
      // has actually successfully worked out
      char *day_arg;
      int year, month, day;
      habit.days_count = 0;

      if (!strcmp(argv[2], "-H")) {
         habit.name = argv[3];
         day_arg = argv[4];
      } else {
         habit.name = default_habit_read();
         day_arg = argv[2];
      }
      sscanf(day_arg, "%d.%d.%d", &year,&month,&day);

      habit_file_read(&habit);
      time_t new_day_timestamp = ymd_to_time_t(year, month, day);
      Day *existing_day = day_exists(&habit, new_day_timestamp);
      if (existing_day != NULL) {
         switch_day_mark(existing_day);
      } else {
         if (date_compare(habit.days[habit.days_count-1].timestamp, new_day_timestamp) != -1)
            habit_add_day(&habit, new_day_timestamp, DAY_COMPLETE);
         else {
            printf("The provided date %d.%02d.%02d is bigger than today\n", year, month, day);
            return 1;
         }

         if (habit.days[habit.days_count-2].timestamp >
             habit.days[habit.days_count-1].timestamp)
            day_sort(habit.days, habit.days_count);
      }
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
   printf("%s\n", filename);

   habit_file = fopen(filename, "r");
   if (habit_file != NULL) {
      fclose(habit_file);
      return 1;
   } else {
      habit_file = fopen(filename, "w");
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
      fprintf(stderr, "file not found\n");
      return 1;
   }

   int day = 0;
   int mark = 0;
   while (fscanf(habit_file, "%d,%d", &day, &mark) != EOF) {
      /*printf("%d,%d\n", day, mark);*/
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
   if (habit->days_count != 0)
      for (int i = 0; i < habit->days_count; i++) {
         fprintf(habit_file, "%ld,%d\n", 
                 habit->days[i].timestamp, 
                 habit->days[i].marked);
      }

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
   struct tm tm1;
   struct tm tm2;
   localtime_r(&ts1, &tm1);
   localtime_r(&ts2, &tm2);

   if (tm1.tm_year == tm2.tm_year &&
       tm1.tm_mon == tm2.tm_mon &&
       tm1.tm_mday == tm2.tm_mday)
   {
      return 0;
   } else if (tm1.tm_year < tm2.tm_year ||
      tm1.tm_mon < tm2.tm_mon ||
      tm1.tm_mday < tm2.tm_mday) {
      return -1;
   } else return 1;

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
   FILE *default_file = fopen(default_habit_path_make(), "w");
   fprintf(default_file, "%s\n", habit->name);
   fclose(default_file);
   return 0;
}

char*
default_habit_read() {
   FILE *default_file = fopen(default_habit_path_make(), "r");
   if (default_file == NULL) {
      return NULL;
   }
   char buffer[1024];
   fgets(buffer, 1024, default_file);

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
      while(arr[j-1].timestamp > arr[j].timestamp) {
            temp = arr[j];
            arr[j] = arr[j-1];
            arr[j-1] = temp;
            j -= 1;
      }
   }
}

void
print_help() {
   printf("ht -a <name> : add new habit\n");
   printf("!TBD! ht -r <name> : remove habit\n");
   printf("!TBD! ht -l        : list habits\n");
   printf("ht -s <name> : select habit as a default one\n");
   printf("ht : mark default habit as completed for today\n");
   printf("     (running this once again will unmark the habit)\n");
   printf("ht -d <YYYY.MM.DD> : mark default habit as completed for any day before today\n");
   printf("     (running this once again will unmark the habit)\n");
   printf("!TBD! ht -H <habit> : mark any habit that isn't default\n");
   printf("!TBD! ht -H <habit> -d <YYYY.MM.DD> : mark any day of the habit\n");
   printf("ht -c : display calender with a default habit\n");
   printf("ht -c <habit> : display calender with a selected habit\n");
   printf("\nUse $%s to select habit path\n", habit_folder_path_env_var);
}

Day*
day_exists(Habit *habit, time_t time) {
   struct tm ti1;
   struct tm ti2;
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
