#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

int run_script(const char *script_content){ // run script.sh
  const char *temp_path = "/tmp/temp_script.sh";
  FILE *fp = fopen(temp_path, "w"); // buka file
  fprintf(fp, "%s", script_content); // tulis script
  fclose(fp);
  chmod(temp_path, 0777); // bikin executable

  // bikin fork
  pid_t pid = fork();
  if (pid == 0) {
    execl("/bin/bash", "bash", temp_path, (char *)NULL);
    exit(1);
  }
  else if (pid > 0) {
    waitpid(pid, NULL, 0);
  }
  unlink(temp_path);
  return 0;
}

void filter_files() { // fiter file
  // membuat directory Filtered
  mkdir("Filtered", 0777); // hak rwx ke semua

  DIR *clues = opendir("./Clues");
  struct dirent *de, *sub;
  if (!clues) {
    perror("DIR Clues tidak ditemukan");
    return;
  }

  char cwd[PATH_MAX];
  getcwd(cwd, sizeof(cwd));  // Ambil path absolut cwd

  while ((de = readdir(clues)) != NULL) {
    if (de->d_type == DT_DIR && strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) {
      char path[PATH_MAX];
      snprintf(path, sizeof(path), "./Clues/%s", de->d_name);
      DIR *subclues = opendir(path);
      if (!subclues) continue;

      while ((sub = readdir(subclues)) != NULL) {
        if (sub->d_type == DT_REG) {

          // file yang dicari: [a-z].txt atau [0-9].txt
          if ( (strlen(sub->d_name) == 5) && ((sub->d_name[0] >= 'a' && sub->d_name[0] <= 'z') || (sub->d_name[0] >= '0' && sub->d_name[0] <= '9'))) {

            char src[PATH_MAX], dest[PATH_MAX];
            snprintf(src, sizeof(src), "%s/%s", path, sub->d_name);
            snprintf(dest, sizeof(dest), "%s/Filtered/%s", cwd, sub->d_name);

            rename(src, dest);
          } else {
            char to_delete[PATH_MAX];
            snprintf(to_delete, sizeof(to_delete), "%s/%s", path, sub->d_name);
            remove(to_delete);
          }
        }
      }

      closedir(subclues);
    }
  }
  closedir(clues);
}

int compare_strings(const void *a, const void *b) { // comparator buat qsort 
    return strcmp(*(const char **)a, *(const char **)b);
}

void combine_files() {
  // buka dir Filtered
  DIR *d = opendir("./Filtered");
  struct dirent *de;
  if (!d){
    perror("Gagal membuka DIR Filtered");
    return;
  }

  // Write COmbined.txt
  FILE *combined = fopen("Combined.txt", "w");
  if (!combined) {
    perror("Failed to open Combined.txt");
    return;
  }

  char *digits[25];
  char *letters[25];
  int dcount=0;
  int lcount=0;
  while ((de = readdir(d)) != NULL){
    if (de->d_type == DT_REG) { 
        char name = de->d_name[0];
      if (isdigit(name)) { // jika nama file digit masuk digits[]
        digits[dcount] = strdup(de->d_name);
        dcount++;
      } else if (isalpha(name)) { // jika nama file alphabet masuk letters[]
        letters[lcount] = strdup(de->d_name);
        lcount++;
      }
    }
  }
  closedir(d); 

  // melakukan qsort untuk tiap array
  qsort(digits, dcount, sizeof(char *), compare_strings);
  qsort(letters, lcount, sizeof(char *), compare_strings);

  int lcounter=0;
  int dcounter=0;
  int state=0;
  while (dcounter < dcount || lcounter < lcount){
    FILE *f = NULL;
    if (state == 0 && dcounter < dcount) {
      char path[1024];
      snprintf(path, sizeof(path), "./Filtered/%s", digits[dcounter]);
      f = fopen(path, "r");
      if (f) {
        char buf[1024];
        while (fgets(buf, sizeof(buf), f)) {
            fputs(buf, combined);
        }
        fclose(f);
      }
      free(digits[dcounter]);
      dcounter++;
      state = 1;
    } else if (state == 1 && lcounter < lcount) {
      char path[1024];
      snprintf(path, sizeof(path), "./Filtered/%s", letters[lcounter]);
      f = fopen(path, "r");
      if (f) {
        char buf[1024];
        while (fgets(buf, sizeof(buf), f)) {
            fputs(buf, combined);
        }
        fclose(f);
      }
      free(letters[lcounter]);
      lcounter++;
      state = 0;
    }
  }

  fclose(combined);
}
void rot13(char *str) {
  for (int i = 0; str[i]; i++) {
    if ('a' <= str[i] && str[i] <= 'z') {
      str[i] = ((str[i] - 'a' + 13) % 26) + 'a';
    } else if ('A' <= str[i] && str[i] <= 'Z') {
      str[i] = ((str[i] - 'A' + 13) % 26) + 'A';
    }
  }
}

void decode_file() {
  FILE *combined = fopen("Combined.txt", "r");
  if (!combined) {
    perror("Failed to open Combined.txt");
    return;
  }

  char buffer[1024];
  FILE *decoded = fopen("Decoded.txt", "w");
  if (!decoded) {
    perror("Failed to open Decoded.txt");
    fclose(combined);
    return;
  }

  while (fgets(buffer, sizeof(buffer), combined)) {
    rot13(buffer);
    fputs(buffer, decoded);
  }

  fclose(combined);
  fclose(decoded);
}
void print_commands(){
  printf("Commands:\n");
  printf("./action -m Filter\n");
  printf("./action -m Combine\n");
  printf("./action -m Decode\n");
}

int main(int argc, char **argv){
  const char *download = // script buat download, unzip, rm file zip
    "#!/bin/bash\n"
    "wget -q -O Clues.zip \"https://drive.usercontent.google.com/u/0/uc?id=1xFn1OBJUuSdnApDseEczKhtNzyGekauK&export=download\"\n"
    "unzip Clues.zip\n"
    "rm Clues.zip\n";

  struct stat st = {0};
  if (!(stat("Clues", &st) == 0 && S_ISDIR(st.st_mode))) { // cek ada dir Clues ga
    run_script(download);
  }

  // cek command yang dijalankan
  // cek apakah arg mencapai tiga kata {./namaprogram, -m, <command>}
  // ke satu dalam array arg (kalau pas diketik urutan ke dua) itu "-m"
  if ( argc == 3 && strcmp(argv[1], "-m") == 0) {
    if (strcmp(argv[2], "Filter") == 0){
      filter_files();
      printf("Files filtering completed");
    } else if (strcmp(argv[2], "Combine") == 0){
      combine_files();
      printf("Files combined successfully");
    } else if (strcmp(argv[2], "Decode") == 0){
      decode_file();
      printf("File decoded successfully");
    } else {
      print_commands();
    }
  } else {
    print_commands();
  }
  return 0;
}
