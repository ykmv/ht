# ht - CLI Habit Tracker
A simple CLI habit tracker written in C. 

![htdemo](htdemo.gif)

## How to compile and install `ht`
```sh
$ ./build.sh -r 
```
Compile the program.

```sh
$ ./build.sh -i
```
Install the program at `~/.local/bin`


## How to use `ht`

```sh
$ ht -a "the name of the habit" # Create a new habit
```

```sh
$ ht -s "the name of the habit" # Select it as a default habit for a quick access.
```

```sh
$ ht # Mark default habit as completed for today.
     # Run this once again to mark today as failed.
     # Run this once more will delete the day.
```

```sh
$ ht -c # Look at the graph of the default habit. 
        # It will show you failed and completed days
$ ht -cl # Look at the list of completed and failed days of the default habit.
```

```sh
$ ht -l # Display the list of all the habits
```

```sh
$ ht -r "the name of the habit" # Remove the habit, 
   # you will have to confirm your decision
```

The default path for storing habits is `~/.local/share/ht`. If you want to change it, use `$HTDIR` environment variable in your shell.

This is the gist of using `ht`. There are some other arguments, if you wish to know them, use `ht -h`.
