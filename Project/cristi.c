#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <fcntl.h>

char getEntryType(mode_t m) {

    if( S_ISREG(m) ) return '-';
    else if ( S_ISDIR(m) ) return 'd';
    else if ( S_ISCHR(m) ) return 'c';
    else if ( S_ISBLK(m) ) return 'b';
    else if ( S_ISFIFO(m) ) return 'p';
    else if ( S_ISSOCK(m) ) return 's';
    else if ( S_ISLNK(m) ) return 'l';
    else return '?';
}

void parseDir(const char* dirName) {

    
    DIR* dir;
    struct dirent* entry;
    struct stat statVar;
    int count=0;

    dir = opendir(dirName);
    if( dir == NULL ) {
        perror("invalid directory");
        exit(2);
    }

    char pathBuf[512];
    struct group *grp;
    struct passwd *pwd;

    while( (entry = readdir(dir)) != NULL ) {

            if( strcmp(entry->d_name,".") == 0 ) {
                perror("skipping current dir");
                continue;
            }
            if( strcmp(entry->d_name,"..") == 0 ) {
                perror("skipping parent dir");
                continue;
            }

            snprintf(pathBuf,sizeof(pathBuf),"%s/%s", dirName, entry->d_name );

            if ( stat(pathBuf,&statVar) == -1 ) {
                perror("stat");
                exit(3);
            }

            pwd = getpwuid(statVar.st_uid);
            const char* owner = ( pwd != NULL ) ? pwd->pw_name : "UNKNOWN";

            grp = getgrgid(statVar.st_gid);
            const char* group = (grp!=NULL) ? grp->gr_name : "UNKNOWN";

                 printf("ENTRY %d: filename => %s\n\t ",++count,entry->d_name);
                 printf("inode => %lu\t hardlinks => %lu\t size => %lu\n\t",statVar.st_ino,statVar.st_nlink,statVar.st_size);
                 printf("TYPE=> %c OWNID: %s GRPID: %s\n",getEntryType(statVar.st_mode),owner,group);

            if( S_ISDIR(statVar.st_mode) ) {
                printf("move to inner dir: \n");
                parseDir( pathBuf );
                printf("return to parent dir:\n");
            }
    }

     closedir(dir);

}

int main(int argc, char** argv) {

    if( argc !=2 ) {
        perror("incorrect call\n Usage: ./executable <dirname>");
        exit(1);
    }
    
    int outFile = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);

    if( outFile == -1 ) {
        perror("opening file");
        exit(errno);
    }

    const char* parentDirName = argv[1];
    parseDir(parentDirName);


    close(outFile);
    return 0;
}