#include<stdio.h>
#include<dirent.h>

int main(int argc, char *argv[])
{
    if(argc!=2)
    {
        perror("nu");
        return -1;
    }
   DIR *dir;
   dir=opendir(argv[1]);
   struct dirent *readdir(dir);
   if(dir!=NULL)
   {
    while(dirent!=NULL)
    {
        printf("%s",dirent->d_name);
    }
   }
   closedir(dir);
}

