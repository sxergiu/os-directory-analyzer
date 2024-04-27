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
#include <libgen.h>
#include <fcntl.h>

#define PERM_LEN 11
#define TIME_LEN 9
#define BUFF_LEN 512
#define NAME_LEN 100

typedef struct _file{

    char* filename;
    long unsigned inode;
    long unsigned filesize;
    long unsigned hlinks;
    char modifTime[TIME_LEN];
    char perms[PERM_LEN];
    char name[NAME_LEN];

    mode_t filemode;
    time_t last_modified;

    bool hasSnapshot;

}File;

//table of contents may be rewritten
//exit error codes may be adjusted
//functions may be split in header files

void printFileInfo(File* f,int out); //prints f info to out file

char getEntryType(mode_t m); //returns char for a certain filemode
void getEntryPerms(File* f, mode_t m); //fills access rights field for file f
void getEntryTime(File* f, time_t t); //fills last modified time field for file f

File* createFile( const char* filepath ); // allocates a structure and inserts metadata based on path

void createSnapshot(File* f, const char* path); //creates snapshot file for file f in the given path
//File* extractSnapshotInfo(const char* snapshotPath); //allocates a structure and inserts metadata based on existing snapshot                                                  //already existing snapshot
//File* findExistingSnapshot(const char* outPath, const char* filename); //looks in the output dir for the snapshot of
                                                                        //the given file and returns it

//bool sameFiles(File* f1, File* f2); //true if equal metadata
//void parseDir(const char* dirPath, const char* outDir, const char* isoDir); //parses each entry of the given path and entries of found subdirectories
                                                        //outDir stores the snapshots
bool isSkippable(const char* filePath); //true if path is crt, parent or a snapshot
DIR* createDir(const char* path);  //opens an existing dir or creates it at given path
bool argsAreOk(int argc, char** argv ); //true if args are ok

void printFileInfo(File* f,int out) {

       dprintf(out,"Filename=>%s\n",f->filename);
       dprintf(out,"Inode=>%lu\nHardlinks=>%lu\nSize(bytes)=>%lu\n",f->inode,f->hlinks,f->filesize);
       dprintf(out,"Last modified=>%s\n", f->modifTime);
       dprintf(out,"Permissions=>%s\n", f->perms);

       close(out);
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

void createSnapshot(File* f, const char* path) {

    char snapshotPath[BUFF_LEN];

    snprintf(snapshotPath, sizeof(snapshotPath), "%s/%s_snapshot.txt", path, f->filename);

    int snapshotFile = open(snapshotPath, O_WRONLY | O_CREAT | O_TRUNC, 0644 );
    
    if (snapshotFile == -1) {
        perror("Unable to create snapshot file");
        exit(6);
    }

    printFileInfo(f,snapshotFile);
}

File extractSnapshotInfo(const char* snapshotPath) {

    int snapshotFile = open(snapshotPath, O_RDONLY);
    File F;
    F.hasSnapshot = true;  

    if (snapshotFile == -1) {
        F.hasSnapshot = false;
        return F;
    }

    char buffer[BUFF_LEN];
    ssize_t bytesRead = read(snapshotFile, buffer, sizeof(buffer));

    if( bytesRead == -1 ) {
        perror("Error reading snapshot file");
        close(snapshotFile);
        exit(-2);
    }

    char* line = strtok(buffer, "\n");
        while (line != NULL) {
            if (strncmp(line, "Filename=>", 10) == 0) {
                sscanf(line, "Filename=>%[^\n]", F.name);
            } else if (strncmp(line, "Inode=>", 7) == 0) {
                sscanf(line, "Inode=>%lu", &F.inode);
            } else if (strncmp(line, "Hardlinks=>", 11) == 0) {
                sscanf(line, "Hardlinks=>%lu", &F.hlinks);
            } else if (strncmp(line, "Size(bytes)=>", 13) == 0) {
                sscanf(line, "Size(bytes)=>%lu", &F.filesize);
            } else if (strncmp(line, "Last modified=>", 15) == 0) {
                sscanf(line, "Last modified=>%[^\n]", F.modifTime);
            } else if (strncmp(line, "Permissions=>", 13) == 0) {
                sscanf(line, "Permissions=>%[^\n]", F.perms);
            }

            line = strtok(NULL, "\n");
        }

    F.modifTime[TIME_LEN-1] = '\0';
    F.perms[PERM_LEN-1] = '\0';
    F.name[NAME_LEN-1] = '\0';
    
    close(snapshotFile);

    return F;
}

File findExistingSnapshot(const char* outPath, const char* filename){
    
    char buf[BUFF_LEN];
    snprintf(buf, BUFF_LEN, "%s/%s_snapshot.txt", outPath, filename);
    return extractSnapshotInfo(buf); 
}

bool sameFiles(File ss, File* f) {

    if ( strcmp( ss.name, f->filename ) ) return false;
    if( ss.filesize != f->filesize ) return false;
    if( ss.inode != f->inode ) return false;
    if( ss.hlinks != f->hlinks ) return false;
    if( strcmp(ss.modifTime,f->modifTime) ) return false;
    if( strcmp(ss.perms,f->perms) ) return false;
    return true;
}

bool isMalware( char* path, const char* iso ) {

    int pipefd[2];
    if( pipe(pipefd) < 0 ) {
        perror("pipe");
        exit(-1);
    } 

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(-10);
    } else if (pid == 0) {
       
        close(pipefd[0]);

        if( dup2(pipefd[1],1) == -1) {
            perror("dup2");
            exit(-10);
        }
        close(pipefd[1]);

        if ( execl("/bin/bash", "/bin/bash", "verify_malware.sh", path, NULL) == -1) {
            perror("execv");
            exit(-11);
        }

    } else {
        
        close(pipefd[1]);

        int status;
        waitpid(pid, &status, 0);
        char buf[BUFF_LEN];

        if (WIFEXITED(status)) {
            
            if ( WEXITSTATUS(status) == 0) {
                
                ssize_t bytesRead = read(pipefd[0],buf, BUFF_LEN );
                if ( bytesRead == -1 ) {
                    perror("read");
                    exit(-10);
                } 
                buf[bytesRead-1] = '\0';

                if( strncmp(buf,"SAFE",bytesRead) == 0 ) {
                       // printf("%s SAFE\n",path);
                        return false;
                }

                char newPath[NAME_LEN];
                snprintf(newPath, sizeof(newPath), "%s/%s", iso , basename(path));
                if (rename(path, newPath) == -1) {
                    perror("rename");
                    exit(-12);
                }
                return true;
            } else {
                perror("isMalware");
                exit(WEXITSTATUS(status));
            }
        }
    }
    return false;
}


void parseDir(const char* dirPath,const char* outPath,const char* isoPath, int* countMalware) {

    DIR* dir;
    struct dirent* entry;
    File* f = NULL;

    dir = opendir(dirPath);
    if( dir == NULL ) {
        perror("invalid directory");
        exit(-1);
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
            
            /*
            if( strstr(f->filename,"noperms") != 0 )
                strncpy(f->perms+1,"---------",PERM_LEN);
            */

            if( strncmp(f->perms+1,"---------",PERM_LEN-1) == 0 ) {
                if ( isMalware(pathBuf,isoPath) ) {
                    printf("!!!file with name %s is potentially dangerous!!!\n", f->filename );
                    (*countMalware)++;
                }
            }
        
            File existingSnapshot = findExistingSnapshot(outPath,f->filename);

            if( existingSnapshot.hasSnapshot == false ){
                printf("+++file with name %s was just added+++\n", f->filename );
                createSnapshot(f,outPath);
            }
            else if( sameFiles(existingSnapshot,f) ) {
               printf("---file with name %s is unchanged---\n", f->filename);
            }
            else {
                printf("~~~file with name %s was modified~~~\n", f->filename);
                createSnapshot(f,outPath);
            }   

            if( S_ISDIR(f->filemode) ) {
                parseDir( pathBuf, outPath, isoPath, countMalware );
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

    if( argc < 6 || argc > 15 || strcmp(argv[1],"-o")!=0  || strcmp(argv[3],"-s") ) {
        perror("incorrect call\n Usage: ./executable -o <output_dir> -s <iso_dir> <dirname1> <dirname2> ... maximum 10 ");
        return false;
    }
    return true;
}

void runParentProcess(int argc,char** argv){

        const char* outputDirPath = argv[2];
        const char* isoDirPath = argv[4];

        DIR* outputDir = createDir(outputDirPath);
        DIR* isoDir = createDir(isoDirPath);

        const char* parentDirName;
        for(int i = 5; i<argc; i++) {
            
            pid_t pid = fork();
            if( pid == -1 ) {
                perror("fork");
                exit(-1);
            } else if ( pid == 0 ) {
                
                int countMalware = 0;
                parentDirName = argv[i];
                parseDir(parentDirName, outputDirPath, isoDirPath, &countMalware );
                printf("Snapshot for directory <%s> created.\n",parentDirName);
                exit(countMalware);
            }
        }

        for(int i=5; i<argc; i++) {

            int status;
            pid_t child_pid = wait(&status);

            if( WIFEXITED(status) ){
                printf("\tChild process #%d with PID %d terminated and found %d malware files.\n\n",i-4, child_pid, WEXITSTATUS(status) );
            }
            else {
                printf("\tChild process #%d with PID %d FAILURE.\n\n",i-4, child_pid);
            }
        }
        closedir(outputDir);
        closedir(isoDir);
}

int main(int argc, char** argv) {

    if( argsAreOk(argc,argv) ) {
            runParentProcess(argc,argv);
    }
    return 0;
}
