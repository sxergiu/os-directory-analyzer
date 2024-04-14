#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <fcntl.h>

#define PERM_LEN 11
#define TIME_LEN 9
#define BUFF_LEN 512

typedef struct _file{

    const char* filename;
    long unsigned inode;
    long unsigned filesize;
    long unsigned hlinks;
    char modifTime[TIME_LEN];
    char perms[PERM_LEN];

    mode_t filemode;
    time_t last_modified;

}File;

void printFileInfo(File* f,FILE* out); //prints f info to out file

char getEntryType(mode_t m); //returns char for a certain filemode
void getEntryPerms(File* f, mode_t m); //fills access rights field for file f
void getEntryTime(File* f, time_t t); //fills last modified time field for file f

File* createFile( const char* filepath ); // allocates a structure and inserts metadata based on path

FILE* createSnapshot(File* f, const char* path); //creates snapshot file for file f in the given path
File* extractSnapshotInfo(const char* snapshotPath); //allocates a structure and inserts metadata based on existing snapshot                                                  //already existing snapshot
File* findExistingSnapshot(const char* outPath, const char* filename); //looks in the output dir for the snapshot of
                                                                        //the given file and returns it

bool sameFiles(File* f1, File* f2); //true if equal metadata
void parseDir(const char* dirPath, const char* outDir); //parses each entry of the given path and entries of found subdirectories
                                                        //outDir stores the snapshots
bool isSkippable(const char* filePath); //true if path is crt, parent or a snapshot
DIR* createDir(const char* path);  //opens an existing dir or creates it at given path
bool argsAreOk(int argc, char** argv ); //true if args are ok


void printFileInfo(File* f,FILE* out) {

       fprintf(out,"ENTRY INFO:\n\nFilename=>%s\n",f->filename);
       fprintf(out,"Inode=>%lu\nHardlinks=>%lu\nSize(bytes)=>%lu\n",f->inode,f->hlinks,f->filesize);
       fprintf(out,"Last modified=>%s\n", f->modifTime);
       fprintf(out,"Permissions=>%s\n", f->perms);

       fclose(out);
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

void getEntryPerms(File* f, mode_t m) {

    snprintf(f->perms, sizeof(f->perms), "%c%c%c%c%c%c%c%c%c%c",
             (getEntryType(m)),
             (m & S_IRUSR) ? 'r' : '-',
             (m & S_IWUSR) ? 'w' : '-',
             (m & S_IXUSR) ? 'x' : '-',
             (m & S_IRGRP) ? 'r' : '-',
             (m & S_IWGRP) ? 'w' : '-',
             (m & S_IXGRP) ? 'x' : '-',
             (m & S_IROTH) ? 'r' : '-',
             (m & S_IWOTH) ? 'w' : '-',
             (m & S_IXOTH) ? 'x' : '-');
}

void getEntryTime(File* f, time_t t) {

    struct tm* tm_info = localtime(&t);
    char time_str[TIME_LEN];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    strncpy(f->modifTime,time_str,TIME_LEN);
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
        exit(1);
    }

     f->inode = statVar.st_ino;
     f->filesize = statVar.st_size;
     f->hlinks = statVar.st_nlink;

     f->filemode = statVar.st_mode;
     getEntryPerms(f,f->filemode);

     f->last_modified = statVar.st_mtime;
     getEntryTime(f,f->last_modified);

     return f;
}

FILE* createSnapshot(File* f, const char* path) {

    char snapshotPath[BUFF_LEN];

    snprintf(snapshotPath, sizeof(snapshotPath), "%s/%s_snapshot.txt", path, f->filename);

    FILE* snapshotFile = fopen(snapshotPath, "w");
    if (snapshotFile == NULL) {
        perror("Unable to create snapshot file");
        exit(6);
    }

    return snapshotFile;    
}


File* extractSnapshotInfo(const char* snapshotPath) {

    FILE* snapshotFile = fopen(snapshotPath, "r");
    if (snapshotFile == NULL) {
        return NULL;
    }

    File* f = malloc(sizeof(File));
    if (f == NULL) {
        perror("Allocation");
        exit(1);
    }

    f->filename = NULL;
    f->inode = 0;
    f->filesize = 0;
    f->hlinks = 0;
    f->last_modified = 0;
    f->filemode = 0;

    char buffer[BUFF_LEN];
    while (!feof(snapshotFile)) {
        if (fgets(buffer, sizeof(buffer), snapshotFile) == NULL) {
            break;
        }

        char* token = strtok(buffer, "=>\n>");
        if (token != NULL) {
            if (strcmp(token, "Filename") == 0) {
                token = strtok(NULL, "=>\n>");
                f->filename = strdup(token);
            } else if (strcmp(token, "Inode") == 0) {
                token = strtok(NULL, "=>\n>");
                f->inode = strtoul(token, NULL, 10);
            } else if (strcmp(token, "Hardlinks") == 0) {
                token = strtok(NULL, "=>\n>");
                f->hlinks = strtoul(token, NULL, 10);
            } else if (strcmp(token, "Size(bytes)") == 0) {
                token = strtok(NULL, "=>\n>");
                f->filesize = strtoul(token, NULL, 10);
            } else if (strcmp(token, "Last modified") == 0) {
                token = strtok(NULL, "=>\n>");
                strncpy(f->modifTime, token, TIME_LEN);
            } else if (strcmp(token, "Permissions") == 0) {
                token = strtok(NULL, "=>\n>");
                strncpy(f->perms, token, PERM_LEN);
            }
        }
    }

    f->modifTime[TIME_LEN-1] = '\0';
    f->perms[PERM_LEN - 1] = '\0';

    fclose(snapshotFile);
    return f;
}


File* findExistingSnapshot(const char* outPath, const char* filename){
    
    char buf[BUFF_LEN];
    snprintf(buf, BUFF_LEN, "%s/%s_snapshot.txt", outPath, filename);
    return extractSnapshotInfo(buf); 
}

bool sameFiles(File* f1, File* f2) {

    if ( strcmp( f1->filename, f2->filename ) ) return false;
    if( f1->filesize != f2->filesize ) return false;
    if( f1->inode != f2->inode ) return false;
    if( f1->hlinks != f2->hlinks ) return false;
    if( strcmp(f1->modifTime,f2->modifTime) ) return false;
    if( strcmp(f1->perms,f2->perms) ) return false;
    return true;
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

    char pathBuf[BUFF_LEN];

    while( (entry = readdir(dir)) != NULL ) {

            if( isSkippable(entry->d_name) ) {
                continue;
            }

            snprintf(pathBuf,sizeof(pathBuf),"%s/%s", dirPath, entry->d_name );

            //insert metadata
            f = createFile(pathBuf); 
            f->filename = entry->d_name;

            File* existingSnapshot = findExistingSnapshot(outPath,f->filename);

            if( existingSnapshot != NULL) {  //snapshot exists

                    if( !sameFiles(existingSnapshot,f) ) { //file was modified

                        printf("~~~file with name %s was modified~~~\n", f->filename);
                    }   //no changes
                    free(existingSnapshot);
            }else { //no snapshot means file is new
                        printf("+++file with name %s was just added+++\n", f->filename );
            }

            FILE* newSnapshot = createSnapshot(f,outPath);
            printFileInfo(f,newSnapshot);

            if( S_ISDIR(f->filemode) ) {
                parseDir( pathBuf, outPath );
            }

            free(f);
    }
     closedir(dir);
}

bool isSkippable(const char* filePath) {
    return strcmp(filePath,".") == 0 || strcmp(filePath,"..") == 0;
}

DIR* createDir(const char* path) {

    DIR* Dir = opendir(path);
    if( Dir == NULL ) {
        if( mkdir( path, 0777 ) != 0 ) {
            perror("failed to create dir!");
            exit(-7);
        } 
    }
    return Dir;
}

bool argsAreOk(int argc, char** argv ) {

    if( argc < 4 || argc > 13 || strcmp(argv[1],"-o")!=0 ) {
        perror("incorrect call\n Usage: ./executable -o <output_dir> <dirname1> <dirname2> ... maximum 10 ");
        return false;
    }
    return true;
}

void runParentProcess(int argc,char** argv){

        const char* outputDirPath = argv[2];
        DIR* outputDir = createDir(argv[2]);

        const char* parentDirName;
        for(int i = 3; i<argc; i++) {
            
            pid_t pid = fork();
            if( pid == -1 ) {
                perror("fork");
                exit(-1);
            } else if ( pid == 0 ) {

                parentDirName = argv[i];
                parseDir(parentDirName, outputDirPath);
                printf("Snapshot for directory <%s> created.\n",parentDirName);
                exit(0);
            }
        }

        for(int i=3; i<argc; i++) {

            int status;
            pid_t child_pid = wait(&status);

            if( WIFEXITED(status) ){
                printf("\tChild process #%d with PID %d terminated and exit code %d.\n\n",i-2, child_pid, WEXITSTATUS(status) );
            }
            else {
                printf("\tChild process #%d with PID %d FAILURE.\n\n",i-2, child_pid);
            }
        }
        closedir(outputDir);
}

int main(int argc, char** argv) {

    if( argsAreOk(argc,argv) ) {
            runParentProcess(argc,argv);
    }
    return 0;
}
