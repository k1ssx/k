#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <sys/stat.h>

#include "source.h"
#include "pkg.h"

void pkg_load(package **head, char *pkg_name) {
    package *new_pkg = (package*) malloc(sizeof(package));    
    package *last = *head;

    if (!new_pkg) {
        printf("error: Failed to allocate memory\n");
        exit(1);
    }

    new_pkg->next = NULL;
    new_pkg->name = pkg_name;
    new_pkg->path = pkg_find(pkg_name);

    if (!*head) {
        new_pkg->prev = NULL;
        *head = new_pkg;
        return;
    }

    while (last->next) {
        last = last->next;
    }

    last->next = new_pkg;
    new_pkg->prev = last;
}


struct version pkg_version(char *repo_dir) {
    struct version version = {0};
    FILE *file;
    char *buf = 0;

    chdir(repo_dir);
    file = fopen("version", "r");

    if (!file) {
        printf("error: version file does not exist\n");
        exit(1);
    }

    getline(&buf, &(size_t){0}, file);
    fclose(file);

    if (!buf) {
        printf("error: version file is incorrect\n");
        exit(1);
    }

    version.version = strtok(buf,    " 	\n");
    version.release = strtok(NULL,   " 	\n");

    if (!version.release) {
        printf("error: release field missing\n");
        exit(1);
    }

    chdir(PWD);

    return version;
}

char **pkg_find(char *pkg_name) {
   char **paths = NULL;
   int  n = 0;
   char cwd[PATH_MAX];
   char **repos = REPOS;

   while (*repos) {
       if (chdir(*repos) != 0) {
           printf("error: Repository not accessible\n");       
           exit(1);
       }

       if (chdir(pkg_name) == 0) {
           paths = realloc(paths, sizeof(char*) * ++n);

           if (paths == NULL) {
               printf("Failed to allocate memory\n");
               exit(1);
           }

           paths[n - 1] =  strdup(getcwd(cwd, sizeof(cwd)));
       }

       ++repos;
   }

   chdir(PWD);
   paths = realloc(paths, sizeof(char*) * (n + 1));
   paths[n] = 0;

   if (*paths) {
       return paths;

   } else {
       printf("error: %s not in any repository\n", pkg_name);
       exit(1);
   }
}

void pkg_list(char *pkg_name) {
    struct version version;
    char *db = "/var/db/kiss/installed"; 
    char *path;
    char cwd[PATH_MAX];

    if (chdir(db) != 0) {
        printf("error: Package db not accessible\n");
        exit(1);
    }

    if (chdir(pkg_name) != 0) {
        printf("error: Package %s not installed\n", pkg_name);
        exit(1);

    } else {
        path = getcwd(cwd, sizeof(cwd)); 
        version = pkg_version(path);
        printf("%s %s %s\n", pkg_name, version.version, version.release);
    }

    chdir(PWD);
}

void pkg_list_all(void) {
    struct version version;
    struct dirent  **list;
    int tot;
    char db[] = "/var/db/kiss/installed";

    if (chdir(db) != 0) {
        printf("error: Failed to access package db\n");
        exit(1);
    }

    tot = scandir(".", &list, NULL, alphasort);

    if (tot == -1) {
        printf("error: Failed to access package db\n");
        exit(1);
    }

    // '2' skips '.'/'..'.
    for (int i = 2; i < tot; i++) {
        if (chdir(list[i]->d_name) == 0) {
            version = pkg_version(list[i]->d_name);    

            printf("%s %s %s\n", list[i]->d_name, \
                    version.version, version.release);
        }

        chdir(db);
    }
}

void pkg_sources(package pkg) {
   char **repos = pkg_find(pkg.name); 
   FILE *file;
   char *lbuf = 0;
   char *source;
   char *source_file;

   chdir(*repos);
   file = fopen("sources", "r");
   
   if (chdir(SRC_DIR) != 0) {
       printf("error: Sources directory not accessible\n"); 
       exit(1);
   }

   if (!file) {
       printf("error: Sources file invalid\n");
       exit(1);
   }

   while ((getline(&lbuf, &(size_t){0}, file)) > 0) {
       // Skip comments and blank lines.
       if ((lbuf)[0] == '#' || (lbuf)[0] == '\n') {
           continue;
       }

       source      = strtok(lbuf, " 	\n");
       source_file = basename(source);

       mkdir(pkg.name, 0777);

       if (chdir(pkg.name) != 0) {
           printf("error: Sources directory not accessible\n");
           exit(1);
       }

       if (access(source_file, F_OK) != -1) {
           printf("%s (Found cached source %s)\n", pkg.name, source_file);
        
       } else if (strncmp(source, "https://", 8) == 0 ||
                  strncmp(source, "http://",  7) == 0) {
           printf("%s (Downloading %s)\n", pkg.name, source);
           source_download(source);

       } else if (chdir(*repos) == 0 && 
                  chdir(dirname(source)) == 0 && 
                  access(source_file, F_OK) != -1) {
           printf("%s (Found local source %s)\n", pkg.name, source_file);

       } else {
           printf("error: No local file %s\n", source);
           exit(1);
       }

       chdir(SRC_DIR);
   }

   fclose(file);
}

void cache_init(void) {
    HOME      = getenv("HOME");
    CAC_DIR   = getenv("XDG_CACHE_HOME");
    char cwd[PATH_MAX];

    if (!HOME || HOME[0] == '\0') {
        printf("HOME directory is NULL\n");
        exit(1);
    }

    if (!CAC_DIR || CAC_DIR[0] == '\0') {
        chdir(HOME);

        mkdir(".cache", 0777);

        if (chdir(".cache") != 0) {
            goto err;
        }
        
        CAC_DIR = strdup(getcwd(cwd, sizeof(cwd)));
    }

    mkdir(CAC_DIR, 0777);

    if (chdir(CAC_DIR) != 0) {
        goto err;
    }

    mkdir("kiss", 0777);

    if (chdir("kiss") != 0) {
        goto err;
    }

    CAC_DIR = strdup(getcwd(cwd, sizeof(cwd)));

    mkdir("build", 0777);
    mkdir("pkg", 0777);
    mkdir("extract", 0777);
    mkdir("sources", 0777);
    mkdir("logs", 0777);  

    if (chdir("build") != 0) {
        goto err;
    }
    MAK_DIR = strdup(getcwd(cwd, sizeof(cwd)));

    if (chdir("../pkg") != 0) {
        goto err;
    }
    PKG_DIR = strdup(getcwd(cwd, sizeof(cwd)));

    if (chdir("../extract") != 0) {
        goto err;
    }
    TAR_DIR = strdup(getcwd(cwd, sizeof(cwd)));

    if (chdir("../sources") != 0) {
        goto err;
    }
    SRC_DIR = strdup(getcwd(cwd, sizeof(cwd)));

    if (chdir("../logs") != 0) {
        goto err;
    }
    LOG_DIR = strdup(getcwd(cwd, sizeof(cwd)));

    if (chdir("../bin") != 0) {
        goto err;
    }
    BIN_DIR = strdup(getcwd(cwd, sizeof(cwd)));

    chdir(PWD);
    return;

err:
    printf("%s\n", "Failed to create cache directory\n");
    exit(1);
}
