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

typedef struct _file{

    const char* filename;
    long unsigned inode;
    long unsigned filesize;
    long unsigned hlinks;
    char filetype;
    const char* owner;
    const char* group;
}File; 

void printFileInfo(File* f,int count,int fdOut) {

       printf("ENTRY %d: filename => %s\n\t ",++count,f->filename);
       printf("inode => %lu\t hardlinks => %lu\t size => %lu\n\t",f->inode,f->hlinks,f->filesize);
       printf("TYPE=> %c OWNID: %s GRPID: %s\n",f->filetype,f->owner,f->group);

    int w;
    w = write(fdOut, &(f->filename), sizeof(f->filename));
    if (w == -1) {
        perror("writing filename to output file");
        exit(errno);
    }
    w = write(fdOut, &(f->inode), sizeof(f->inode));
    if (w == -1) {
        perror("writing inode to output file");
        exit(errno);
    }
    w = write(fdOut, &(f->filesize), sizeof(f->filesize));
    if (w == -1) {
        perror("writing filesize to output file");
        exit(errno);
    }
    w = write(fdOut, &(f->hlinks), sizeof(f->hlinks));
    if (w == -1) {
        perror("writing hardlinks to output file");
        exit(errno);
    }
    w = write(fdOut, &(f->filetype), sizeof(f->filetype));
    if (w == -1) {
        perror("writing filetype to output file");
        exit(errno);
    }
    w = write(fdOut, &(f->owner), sizeof(f->owner));
    if (w == -1) {
        perror("writing owner to output file");
        exit(errno);
    }
    w = write(fdOut, &(f->group), sizeof(f->group));
    if (w == -1) {
        perror("writing group to output file");
        exit(errno);
    }
} 

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

void parseDir(const char* dirName,int outputFile) {

    
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

            File f;
            f.filename = entry->d_name;
            f.inode = statVar.st_ino;
            f.filesize = statVar.st_size;
            f.filetype = getEntryType(statVar.st_mode);
            f.hlinks = statVar.st_nlink;
            f.owner = owner;
            f.group = group;

            printFileInfo(&f,count++,outputFile);

            if( S_ISDIR(statVar.st_mode) ) {
                printf("move to inner dir: \n");
                parseDir( pathBuf, outputFile );
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
    
    int outFile = open("output.bin", O_WRONLY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);

    if( outFile == -1 ) {
        perror("opening file");
        exit(errno);
    }

    const char* parentDirName = argv[1];
    parseDir(parentDirName,outFile);

    close(outFile);
    return 0;
}
