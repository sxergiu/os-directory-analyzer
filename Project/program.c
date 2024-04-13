#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
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
    //time?
    //acces rights

    mode_t filemode;

}File;

//function that creates a file with info extracted from a snapshot
//function that compares 2 file structures
//overwriting mechanism
//add relevant file field + remove user and group id
//>>>create snapshots within child processes

DIR* createDir(const char* path);  //opens an existing dir or creates it at given path
bool isSkippable(const char* filePath); //checks if path is crt or parent
void printFileInfo(File* f,FILE* out); //prints f info to out file
FILE* createSnapshot(File* f, const char* path); //creates snapshot file for file f in the given path
char getEntryType(mode_t m); //converts filemode to readable type
File* createFile( const char* filepath ); // allocates a structure and inserts metadata based on path
void parseDir(const char* dirPath, const char* outDir); //parses each entry of the given path and entries of found subdirectories
                                                        //outDir stores the snapshots
void printFileInfo(File* f,FILE* out) {

       fprintf(out,"ENTRY INFO:\n\t filename => %s\n\t ",f->filename);
       fprintf(out,"inode => %lu\n\t hardlinks => %lu\n\t size => %lu\n\t",f->inode,f->hlinks,f->filesize);
       fprintf(out,"TYPE=> %c OWNID: %s GRPID: %s\n",f->filetype,f->owner,f->group);
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

File* createFile( const char* filepath ) {

    struct stat statVar;

    if ( stat(filepath,&statVar) == -1 ) {
                perror("stat");
                exit(3);
    }

    File* f = malloc(sizeof(File));
    if( f == NULL ) {
        perror("Allocation");
    }

    struct group *grp;
    struct passwd *pwd;

    pwd = getpwuid(statVar.st_uid);
    const char* owner = ( pwd != NULL ) ? pwd->pw_name : "UNKNOWN";

    grp = getgrgid(statVar.st_gid);
    const char* group = (grp!=NULL) ? grp->gr_name : "UNKNOWN";

     f->inode = statVar.st_ino;
     f->filesize = statVar.st_size;
     f->filetype = getEntryType(statVar.st_mode);
     f->hlinks = statVar.st_nlink;
     f->owner = owner;
     f->group = group;
     f->filemode = statVar.st_mode;

     return f;
}

FILE* createSnapshot(File* f, const char* path) {

    char snapshotPath[512];

    snprintf(snapshotPath, sizeof(snapshotPath), "%s/%s_snapshot.txt", path, f->filename);

    FILE* snapshotFile = fopen(snapshotPath, "w");
    if (snapshotFile == NULL) {
        perror("Unable to create snapshot file");
        exit(6);
    }

    return snapshotFile;    
}

void parseDir(const char* dirPath,const char* outPath) {

    DIR* dir;
    struct dirent* entry;
    File* f = NULL;

    dir = opendir(dirPath);
    if( dir == NULL ) {
        perror("invalid directory");
        exit(2);
    }

    char pathBuf[512];

    while( (entry = readdir(dir)) != NULL ) {

            if( isSkippable(entry->d_name) ) {
                continue;
            }

            snprintf(pathBuf,sizeof(pathBuf),"%s/%s", dirPath, entry->d_name );

            //insert metadata
            f = createFile(pathBuf); 
            f->filename = entry->d_name;

            //print metadata
            //printf("%s\n",f->filename);
            FILE* snapshotFile = createSnapshot(f,outPath);
            printFileInfo(f,snapshotFile);
            fclose(snapshotFile);

            if( S_ISDIR(f->filemode) ) {
                parseDir( pathBuf, outPath );
            }

            free(f);
    }

     closedir(dir);

}

bool isSkippable(const char* filePath) {
    return strcmp(filePath,".") == 0 || strcmp(filePath,"..") == 0 || strstr(filePath,"snapshot");
}

DIR* createDir(const char* path) {

    DIR* Dir = opendir(path);
    if( Dir == NULL ) {
        if( mkdir( path, 0777 ) != 0 ) {
            perror("failed to create dir!");
            exit(1);
        } 
    }
    return Dir;
}

bool argsAreOk(int argc, char** argv ) {

    if( argc < 2 || argc > 13 || strcmp(argv[1],"-o")!=0 ) {
        perror("incorrect call\n Usage: ./executable -o <output_dir> <dirname1> <dirname2> ... maximum 10 ");
        return false;
    }
    return true;
}

void run(int argc,char** argv){

         const char* outputDirPath = argv[2];
         DIR* outputDir = createDir(argv[2]);

        const char* parentDirName;
        for(int i = 3; i<argc; i++) {

            parentDirName = argv[i];
            parseDir(parentDirName, outputDirPath);
        }

        closedir(outputDir);
}

int main(int argc, char** argv) {

    if( argsAreOk(argc,argv) ) {
            run(argc,argv);
    }
    return 0;
}
