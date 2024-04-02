#include<stdio.h>
#include<dirent.h>

int main(int argc, char *argv[])
{
    if(argc!=2)
    {
        perror("nu");
        return 1;
    }
   DIR *dir;
   dir=opendir(argv[1]);
   if(dir==NULL)
   {
    return 1;
    perror("nu");

   }
   struct dirent *en;
   while((en=readdir(dir))!=NULL)

    {
        printf("%s",en->d_name);
    }
   closedir(dir);
}

