#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>

#define _XOPEN_SOURCE ;

int recursion = 0; //1 if enabled, otherwise 0
int sleepTime = 300;
int fileLimit = 0;
int signaL = 0;
int exitSignal = 0;
int buffer = 1000;

//Returns 0 if arguments are correct otherwise returns 1
int readArguments(int number, char **argv, char *source, char *goal);
int checkFileType(struct stat file);
int copy(char *source, char *target, mode_t mask);
int copy_map(char *source, char *target, struct stat *Source);
void syncCopy(char *source, char *target);
void syncRemove(char *source, char *target);
void my_handler(int sig);
void exitFunction(int sig);

int main(int argc, char **argv)
{
   //char tables for paths
   char source[500], goal[500];
   struct stat Source, Goal;
   struct sigaction my_action, old_action;

   //checking and reading arguments
   if (readArguments(argc, argv, source, goal) == 1)
       exit(-1);

   //checking paths

   //checking  if argv[1] and argv[2] are existing paths
   if (lstat(source, &Source) != 0 || lstat(goal, &Goal) != 0) //bad result
   {
       printf("One of the paths or both dont exist\n");
       exit(-1);
   }

   if (checkFileType(Source) != 0)
   {
       printf("Source path is not path to folder");
       exit(-1);
   }

   if (checkFileType(Goal) != 0)
   {
       printf("Goal path is not path to folder");
       exit(-1);
   }

   //forking the parent process
   pid_t pid;
   // Fork off the parent process  and create new
   pid = fork();
   //if failure
   if (pid < 0)
   {
       exit(-1);
   }
   // if it is native process
   else if (pid > 0)
   {
       return 0;
   }
   //if pid==0 then it is childs process

   //now we have to umask in order to write to any files(for exmaple logs)

   umask(0);
   openlog("logFile", LOG_PID, LOG_DAEMON);
   syslog(LOG_INFO, "Deamon has just started running\n");

   pid_t sid = setsid();
   if (sid < 0)
   {
       syslog(LOG_ERR, "Error with session opening\n");
       exit(-1);
   }

   //SIGNAL SIGUSR1
   my_action.sa_handler = my_handler;
   sigfillset(&my_action.sa_mask);
   my_action.sa_flags = 0;
   if (sigaction(SIGUSR1, &my_action, &old_action) < 0)
   {
       syslog(LOG_ERR, "Error with the use of  SIGUSR1 signal\n");
       exit(-1);
   }

   //SIGNAL SIGUSR2 for exiting daemon
   my_action.sa_handler = exitFunction;
   sigfillset(&my_action.sa_mask);
   my_action.sa_flags = 0;
   if (sigaction(SIGUSR2, &my_action, &old_action) < 0)
   {
       syslog(LOG_ERR, "Error with the use of  SIGUSR2 signal\n");
       exit(-1);
   }

   while (!exitSignal)
   {
       sleep(sleepTime);
       switch (signaL)
       {
       case 0:
           syslog(LOG_INFO, "Demon started working after %ds\n", sleepTime);
           break;

       case 1:
       {
           syslog(LOG_INFO, "Demon started working after SIGUSR1 signal\n");
           signaL = 0; //Need to reeset signaL
           break;
       }
       }

       syncCopy(source, goal);
       syncRemove(source, goal);
       syslog(LOG_INFO, "Demon has just  gone to sleep");
   }

   //at the end of program we need to close log using
   syslog(LOG_INFO, "Demon has stopped\n");
   closelog();

   return 0;
}

int readArguments(int number, char **argv, char *source, char *goal)
{
   //if number of arguments is less than minimum or greater than maximum
   if (number < 3 || number > 9)
   {
       printf("Incorrect number of arguments\n");
       return 1;
   }

   else
   {
       //for obligatory arguments(paths will be checked later in main)
       //paths copy
       strcpy(source, argv[1]);
       strcpy(goal, argv[2]);

       int i, variable;
       //Let's assume for example when we have -T 200 that -T is argument and 200 is value
       //0 Initial values for guards
       //2 guards that argument can be read only once
       int recursionGuard = 0, timeGuard = 0, limitGuard = 0;

       //checks each optional argument separately
       for (i = 3; i < number; ++i)
       {
           //recognises arguments and converts value from string into int
           if (strcmp(argv[i], "-T") == 0 && timeGuard == 0)
           {
               //if it is in range of argv array we can read value of -T
               if (argv[i + 1] > 0)
               {
                   i = i + 1;
                   variable = atoi(argv[i]);
                   if (variable > 0) //if atoi can convert string into int
                   {
                       sleepTime = variable;
                       timeGuard = 2;
                   }
                   //if convertion failed
                   else
                   {
                       printf("Incorrect argument types\n");
                       return 1;
                   }
               }
               //if it is not  in range of argv  array,like only -T
               else
               {
                   printf("Incorrect number of arguments\n");
                   return 1;
               }
           }
           else if (strcmp(argv[i], "-R") == 0 && recursionGuard == 0)
           {
               recursion = 1;
               recursionGuard = 2;
           }
           else if (strcmp(argv[i], "-M") == 0 && limitGuard == 0)
           {

               //if it is in range of argv array we can read value of -M
               if (argv[i + 1] > 0)
               {
                   i = i + 1;
                   variable = atoi(argv[i]);
                   if (variable > 0) //if atoi can convert string into int
                   {
                       fileLimit = variable;
                       limitGuard = 2;
                   }
                   else
                   {
                       printf("Incorrect argument types\n");
                       return 1;
                   }
               }
               //if it is not  in range of argv  array,like only -M
               else
               {
                   printf("Incorrect number of arguments\n");
                   return 1;
               }
           }
           else
           {
               printf("Problem with reading arguments.Enter arguments correctly.\n [daemon name] [path to source] [path to goal] [-R for recursive folder sync]\n [-T with value(integer) for change deamon sleep] [-M with value(integer) for change maximum file size to copy] ");
               return 1;
           }
       }
   }
   return 0;
}

int checkFileType(struct stat file)
{
   if (S_ISDIR(file.st_mode))
       return 0; //folder
   else if (S_ISREG(file.st_mode))
       return 1; //regular file
   else
       return -1; //something different
}


//copy function
int copy(char *source, char *target, mode_t mask)
{
   int fin, fout;
   int count_R;
   char bufor[buffer];

   //try to open source file

   if ((fin = open(source, O_RDONLY)) < 0)
   {
       syslog(LOG_ERR, "Failed to open file: %s\n", source);
       return 0;
   }

   //try to open target file

   if ((fout = open(target, O_CREAT | O_WRONLY | O_TRUNC, mask)) < 0)
   {
       syslog(LOG_ERR, "Failed to open file: %s\n", target);
       return 0;
   }

   //try to copy

   while (count_R = read(fin, bufor, buffer))
   {
       if (count_R < 0)
       {
           syslog(LOG_ERR, "Failed to read from file: %s\n", source);
           close(fin);
           close(fout);
           return 0;
       }
       if (write(fout, bufor, count_R) < 0)
       {
           syslog(LOG_ERR, "Failed to write to file: %s\n", target);
           close(fin);
           close(fout);
           return 0;
       }
   }
   close(fin);
   close(fout);
   return 1;
}

//copy function using mapping

int copy_map(char *source, char *target, struct stat *Source)
{
   int fin, fout;
   char *pom;

   //try to open source file

   if ((fin = open(source, O_RDONLY)) < 0)
   {
       syslog(LOG_ERR, "Failed to open file: %s\n", source);
       return 0;
   }

   //try to open target file

   if ((fout = open(target, O_CREAT | O_WRONLY | O_TRUNC, Source->st_mode)) < 0)
   {
       syslog(LOG_ERR, "Failed to open file: %s\n", target);
       return 0;
   }

   //try to copy

   if ((pom = mmap(0, Source->st_size, PROT_READ, MAP_SHARED, fin, 0)) < 0)
   {
       syslog(LOG_ERR, "Failed to read from file: %s\n", source);
       close(fin);
       close(fout);
       return 0;
   }
   if (write(fout, pom, Source->st_size) < 0)
   {
       syslog(LOG_ERR, "Failed to write to file: %s\n", target);
       close(fin);
       close(fout);
       return 0;
   }
   munmap(pom, Source->st_size);
   close(fin);
   close(fout);
   return 1;
}

void syncCopy(char *source, char *target)
{
   char src_tmp[500];          //source's path
   char trgt_tmp[500];         //target's path
   struct stat TRGT, SRC;     
   DIR *folder;
   struct dirent *dptr = malloc(sizeof(struct dirent));
   ;
   //opening folder
   if ((folder = opendir(source)) == NULL)
   {
       syslog(LOG_ERR, "Unsuccesful attempt of opening file %s.\n", source);
       return;
   }
   //reading folder
   while ((dptr = readdir(folder)) != NULL)
   {
       if ((strcmp(dptr->d_name, ".") == 0) || (strcmp(dptr->d_name, "..") == 0))
           continue;

       strcpy(src_tmp, source);
       strcat(src_tmp, "/");
       strcat(src_tmp, dptr->d_name);

       strcpy(trgt_tmp, target);
       strcat(trgt_tmp, "/");
       strcat(trgt_tmp, dptr->d_name);

       lstat(src_tmp, &SRC);  //checking paths
       if (checkFileType(SRC) > 0)  //checking type of source
       {
           if (lstat(trgt_tmp, &TRGT) == 0) // file exists
           {
               if (checkFileType(TRGT) > 0)  //checking type of target
                   if ((TRGT.st_mtime - SRC.st_mtime) < 0) // file was modified
                   {
                       if ((fileLimit != 0) && (fileLimit > SRC.st_size))
                       {
                           if (copy(src_tmp, trgt_tmp, SRC.st_mode))
                           {
                               syslog(LOG_INFO, "Copying the file %s was succesful.\n", dptr->d_name);
                           }
                           else
                           {
                               syslog(LOG_ERR, "Unsuccesful attempt of copying file %s.\n", dptr->d_name);
                           }
                       }
                       else
                       {
                           if (copy_map(src_tmp, trgt_tmp, &SRC))
                           {
                               syslog(LOG_INFO, "Copying the file %s was succesful.\n", dptr->d_name);
                           }
                           else
                           {
                               syslog(LOG_ERR, "Unsuccesful attempt of copying file %s.\n", dptr->d_name);
                           }
                       }
                   }
           }
           else
           {
               if ((fileLimit != 0) && (fileLimit > SRC.st_size))
               {
                   if (copy(src_tmp, trgt_tmp, SRC.st_mode))
                   {
                       syslog(LOG_INFO, "Copying the file %s was succesful.\n", dptr->d_name);
                   }
                   else
                   {
                       syslog(LOG_ERR, "Unsuccesful attempt of copying file %s.\n", dptr->d_name);
                   }
               }
               else
               {
                   if (copy_map(src_tmp, trgt_tmp, &SRC))
                   {
                       syslog(LOG_INFO, "Copying the file %s was succesful.\n", dptr->d_name);
                   }
                   else
                   {
                       syslog(LOG_ERR, "Unsuccesful attempt of copying file %s.\n", dptr->d_name);
                   }
               }
           }
       }
       if (checkFileType(SRC) == 0 && recursion)
       {
           if (lstat(trgt_tmp, &TRGT) == 0)
           {
               if (checkFileType(TRGT) == 0)
                   syncCopy(src_tmp, trgt_tmp);
           }
           else if (mkdir(trgt_tmp, SRC.st_mode) == 0)
           {
               syslog(LOG_INFO, "Creating the folder %s was succesful.\n", trgt_tmp);
               syncCopy(src_tmp, trgt_tmp);
           }
           else
               syslog(LOG_ERR, "Unsuccesful attempt of creating folder %s.\n", trgt_tmp);
       }
   }
   closedir(folder);
   free(dptr);
}

void syncRemove(char *source, char *target)
{
   char src_tmp[100];
   char trgt_tmp[100];
   struct stat TRGT, SRC;
   DIR *folder;
   struct dirent *dptr = malloc(sizeof(struct dirent));

   if ((folder = opendir(target)) == NULL)
   {
       syslog(LOG_ERR, "Unsuccesful attempt of opening file %s.\n", target);
       return;
   }

   while ((dptr = readdir(folder)) != NULL)
   {
       if ((strcmp(dptr->d_name, ".") == 0) || (strcmp(dptr->d_name, "..") == 0))
           continue;

       strcpy(src_tmp, source);
       strcat(src_tmp, "/");
       strcat(src_tmp, dptr->d_name);

       strcpy(trgt_tmp, target);
       strcat(trgt_tmp, "/");
       strcat(trgt_tmp, dptr->d_name);

       lstat(trgt_tmp, &TRGT);
       if (checkFileType(TRGT) > 0)
       {
           if (lstat(src_tmp, &SRC) == 0)
           {
               if (checkFileType(SRC) != 1)
                   if (unlink(trgt_tmp) == 0)
                       syslog(LOG_INFO, "Removing the file %s was succesful.\n", trgt_tmp);
                   else
                       syslog(LOG_ERR, "Unsuccesful attempt of removing file %s.\n", trgt_tmp);
           }
           else
           {
               if (unlink(trgt_tmp) == 0)
                   syslog(LOG_INFO, "Removing the file %s was succesful.\n", trgt_tmp);
               else
                   syslog(LOG_ERR, "Unsuccesful attempt of removing file %s.\n", trgt_tmp);
           }
       }
       if (checkFileType(TRGT) == 0 && recursion)
       {
           if (lstat(src_tmp, &SRC) == 0)
           {
               if (checkFileType(SRC) == 0)
                   syncRemove(src_tmp, trgt_tmp);
           }
           else
           {
               syncRemove(src_tmp, trgt_tmp);
               if (rmdir(trgt_tmp) == 0)
                   syslog(LOG_INFO, "Removing the folder %s was succesful.\n", trgt_tmp);
               else
                   syslog(LOG_ERR, "Unsuccesful attempt of removing file %s.\n", trgt_tmp);
           }
       }
   }
   closedir(folder);
   free(dptr);
}

void my_handler(int sig)
{
    syslog(LOG_INFO, "Daemon received signal SIGUSR1\n");
    signaL = 1;
}

void exitFunction(int sig)
{
    syslog(LOG_INFO, "Daemon received signal SIGUSR2\n");
    exitSignal = 1;
}
