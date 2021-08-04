#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include "rabin.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

// 1MiB buffer
unsigned long long sum = 0;
char src_dir[1000] = "/Users/ijeong-eun/Desktop/rabin-cdc";

long long unsigned int hash_arr[10000000];
int h_index = 0;


bool exist(long long unsigned int input){
  for(int i=0;i<h_index;i++){
    if(hash_arr[i] == input){
//      printf("dup!!\n");
      return true;
    }
    
  }
  return false;
}

void add_to_ht(long long unsigned int input){
  hash_arr[h_index++]=input;
}

void FP(char input_path[10000]){

        uint8_t buf[1024*1024];
        size_t bytes = 0;
        size_t duplicated = 0;
        printf("input path = %s\n", input_path);

        struct rabin_t *hash;
        hash = rabin_init();

        unsigned int chunks = 0;
        FILE* in_file;
        in_file = fopen(input_path, "r");
        if(in_file == NULL) {
          printf("file open error\n");
          return;
        }
        
        while (!feof(in_file)) {
            size_t len = fread(buf, 1, sizeof(buf), in_file);
            uint8_t *ptr = &buf[0];

            bytes += len;
            while (1) {
                int remaining = rabin_next_chunk(hash, ptr, len);
                if (remaining < 0) {
                    break;
                }

                len -= remaining;
                ptr += remaining;

                printf("%d %016llx\n",
                    last_chunk.length,
                    (long long unsigned int)last_chunk.cut_fingerprint);
                if(!exist(last_chunk.cut_fingerprint)){
                  printf("add to ht\n");
                  add_to_ht(last_chunk.cut_fingerprint);
                }else{
                  duplicated += last_chunk.length;
                  printf("duplicated = %u\n",duplicated);
                }
                chunks++;
            }
        }

        if (rabin_finalize(hash) != NULL) {
            chunks++;
            printf("%d %016llx\n",
                last_chunk.length,
                (long long unsigned int)last_chunk.cut_fingerprint);

            if(!exist(last_chunk.cut_fingerprint)){
              add_to_ht(last_chunk.cut_fingerprint);
            }
            else{
              duplicated += last_chunk.length;
            }
        }

        unsigned int avg = 0;
        if (chunks > 0)
            avg = bytes/chunks;
        printf("%d chunks, average chunk size %d\n", chunks, avg);
        
        //unsigned double d_ratio=1;
        if ( bytes > 0 && duplicated > 0){
//            d_ratio =(long)duplicated/bytes;            
            printf("bytes = %u\n duplicated = %u\n d_ratio = %.3f\n",bytes, duplicated, (double)duplicated/(double)bytes * 100);
        }
        fclose(in_file);
        sum += chunks;
}

void read_recursive(DIR* input_dir, char input_path[10000]){

    struct dirent* entry = NULL;
    while( (entry = readdir(input_dir)) != NULL) {
        char temp[10000] = {' '};
        struct stat s_buf;
        lstat(entry->d_name, &s_buf);
    
        if(entry->d_type == DT_REG){
          printf("FILE = %s \n", entry->d_name);
          
          strcpy(temp, input_path);
          strcat(temp, "/");
          strcat(temp, entry->d_name);
          FP(temp);
        }
        else if(entry->d_type == DT_DIR) {
          printf("DIR = %s \n", entry->d_name);
          if(entry->d_name[0] != '.'){
            strcpy(temp, input_path);
            strcat(temp, "/");
            strcat(temp, entry->d_name);
            printf("%s\n",temp);
            DIR* dir1 = opendir(temp);
            if(dir1 == NULL){
              printf("error\n");
              FP(temp);
              continue;
            }
            read_recursive(dir1, temp);
            closedir(dir1);
          }
        }

    }
}

int main(void) {
    struct rabin_t *hash;
    hash = rabin_init();


    unsigned int chunks = 0;


    DIR* dir = opendir(src_dir);
    struct dirent* entry = NULL;
    FILE* in_file;
   

    size_t a = 10001111;
    size_t b = 101010;
    
    read_recursive(dir, src_dir);
    closedir(dir);
    printf("sum = %ld\n", sum);
    printf("h_index = %d\n", h_index);
    return 0;
}
