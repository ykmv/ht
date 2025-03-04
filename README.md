# ht - CLI Habit Tracker
A simple CLI habit tracker written in C. 
(The project is still in development)

```
ht -h : print this message
ht -a <name> : add new habit
!TBD! ht -r <name> : remove habit
!TBD! ht -l        : list habits
ht -s <name> : select habit as a default one
ht -s : unmark habit as default one (if it it was selected beforehand
ht : mark default habit as completed for today
     (running this once again will unmark the habit)
ht <YYYY.MM.DD> : mark default habit as completed for any day before today
     (running this once again will unmark the habit)
ht -H <habit> : mark any habit that isn't default
ht -H <habit> <YYYY.MM.DD> : mark any day of the habit
ht -c : display calender with a default habit
ht -c <habit> : display calender with a selected habit

Use $HTDIR to select habit path
This help message is displayed when no default habit was selected
```
