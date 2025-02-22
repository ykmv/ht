#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// A Habit Tracker
//
// Each habit is a separate CSV file with a name of habit
// as the name of the file. Structure of CSV:
//   Day,Marked

// marked = -1 if day is marked as not completed
// marked =  0 if day is not marked at all
// marked =  1 if day is marked as completed

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

const char *habit_path;
const char *default_habit_filename = "default";

char *habit_create_arg = "-a";
char *habit_remove_arg = "-r";
char *habit_list_arg = "-l";
char *habit_select_default_arg = "-s";
char *habit_folder_select_arg = "-f";
char *habit_display_arg = "-c";

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

int
main(int argc, char *argv[]) {
   if (argc <= 2) {
      FILE *default_file = fopen(default_habit_filename, "r");
      if (default_file == NULL) {
         print_help();
         return 0;
      }
      fclose(default_file);

      Habit habit = {
         .name = default_habit_read(),
         .days_count = 0,
      };
      habit_file_read(&habit);

      if (habit.days_count == 0) {
         habit_add_today(&habit, DAY_COMPLETE); 
      }
      else if (date_compare(
                  habit.days[habit.days_count-1].timestamp, 
                  time(NULL)
               ) == 0) {
         if (habit.days[habit.days_count-1].marked == DAY_UNMARKED ||
             habit.days[habit.days_count-1].marked == DAY_FAILED)
               habit.days[habit.days_count-1].marked = DAY_COMPLETE;
         else habit.days[habit.days_count-1].marked = DAY_FAILED;
      } else habit_add_today(&habit, DAY_COMPLETE); 

      habit_file_write(&habit, "w");
      free(habit.days);
      return 0;
   } 

   if (!strcmp(argv[1], habit_create_arg)) {
      Habit habit = {
         .name = argv[2]
      };

      // TODO: look at the <errno.h> library
      int err = habit_file_create(&habit);
      if (err) {
         fprintf(stderr, "habit already exists\n");
         return 1;
      } else {
         printf("Habit created\n");
      }
   } else if (!strcmp(argv[1], habit_select_default_arg)) {
      Habit habit = {
         .name = argv[2]
      };
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
      Habit habit = {
         .name = argv[2],
         .days_count = 0,
      };
      habit_file_read(&habit);
      for (int i = 0; i < habit.days_count; i++) {
         struct tm *timeinfo = localtime(&habit.days[i].timestamp);
         printf("day: %d.%d.%d marked: %d\n", 
                timeinfo->tm_year + 1900,
                timeinfo->tm_mon,
                timeinfo->tm_mday,
                habit.days[i].marked);
      }
   }

   /*for (int i = 0; i < habit.days_count; i++) {*/
   /*   struct tm *timeinfo;*/
   /*   timeinfo = localtime(&habit.days[i].timestamp);*/
   /**/
   /*   printf("day %d: time: %d.%d.%d marked: %d\n", */
   /*          i+1,*/
   /*          timeinfo->tm_year + 1900,*/
   /*          timeinfo->tm_mon + 1,*/
   /*          timeinfo->tm_mday,*/
   /*          habit.days[i].marked);*/
   /*}*/

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
         fprintf(habit_file, "%d,%d\n", 
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
   }
   return 1;
}

char*
habit_make_filename(Habit *habit) {
   char *filename = malloc(strlen(habit->name) + sizeof(char) * 5);
   strcpy(filename, habit->name);
   strcat(filename, ".csv");
   return filename;
}

int
default_habit_write(Habit *habit) {
   FILE *default_file = fopen(default_habit_filename, "w");
   fprintf(default_file, "%s\n", habit->name);
   fclose(default_file);
   return 0;
}

char*
default_habit_read() {
   FILE *default_file = fopen(default_habit_filename, "r");
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
   printf("ht -r <name> : remove habit\n");
   printf("ht -l        : list habits\n");
   printf("ht -s <name> : select habit as a default one\n");
   printf("ht -f <path> : select habit folder\n");
   printf("ht : mark default habit as completed for today\n");
   printf("     (running this once again will unmark the habit)\n");
   printf("ht -d <YYYY.MM.DD> : mark default habit as completed for any day before today\n");
   printf("     (running this once again will unmark the habit)\n");
   printf("ht -H <habit> : mark any habit that isn't default\n");
   printf("ht -H <habit> -d <YYYY.MM.DD> : mark any day of the habit\n");
   printf("ht -c : display calender with a default habit\n");
   printf("ht -c <habit> : display calender with a selected habit\n");
}
