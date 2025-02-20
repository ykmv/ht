#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// A Habit Tracker
//
// Each habit is a separate CSV file with a name of habit
// as the name of the file. Structure of CSV:
//   Day,Marked
//
// Commands:
// ht -a <name> : add new habit
// ht -r <name> : remove habit
// ht -l        : list habits
// ht -s <name> : select habit as a default one
// ht -f <path> : select habit folder
// ht : mark selected habit as completed for today
//      (running this once again will unmark the habit)
// ht -d : display calender with a habit

// marked = -1 if day is marked as not completed
// marked =  0 if day is not marked at all
// marked =  1 if day is marked as completed
typedef struct {
   time_t time;
   signed int marked;
} Day;


typedef struct {
   char *name;
   Day *days[];
} Habit;

const char *habit_path;

int habit_file_create(Habit *habit);
int habit_file_read(Habit *habit);
int habit_mark_day(Day *day, signed int status);

int
main(int argc, char *argv[]) {
   for (int i = 0; i < argc; i++) {
      printf("%d %s\n", i, argv[i]);
   }

   return 0;
}

int
habit_file_create(Habit *habit) {
   FILE *habit_file;
   char *filename = malloc(strlen(habit->name) + sizeof(char) * 5);
   strcpy(filename, habit->name);
   strcat(filename, ".csv");

   habit_file = fopen(filename, "r");
   if (habit_file != NULL) {
      fclose(habit_file);
      return 1;
   } else {
      habit_file = fopen(filename, "w");

      fprintf(habit_file, "day,complete");

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
   return 0;
}

int 
habit_mark_day(Day *day, signed int status) {
   return 0;
}
