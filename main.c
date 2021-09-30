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
#include <time.h>

#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <atomic>
#include <unordered_map>
#include <sstream>

#include "sha1.h"

#define THREAD_NUM 1

using namespace std;

unsigned long long sum_chunks = 0;
unsigned long long sum_bytes = 0;
unsigned long long sum_dup = 0;
unsigned long long sample_chunks = 0;
unsigned long long sample_bytes = 0;
unsigned long long sample_dup = 0;
unsigned long long dup_origin = 0;
char src_dir[1000] = "/home/cephlab/mnt/for_dedup/dedup_data/all";

int f_cnt = 0;
int ref_cnt_file = 0;
int max_ref_cnt_file = 0;

unordered_map<string, int> fp_map;
unordered_map<string, int> s_fp_map;


int h_index = 0;
int s_h_index = 0;
int d_ratio_record[101];
int s_d_ratio_record[101];
unsigned int sum_files = 0;
int sample_files = 0;

int ref_cnt_dis[10000000];
int s_ref_cnt_dis[10000000];
int t_ref_cnt_dis[10000000];

struct fp_type{
  unsigned char fingerprint[20];
  bool sample;
  size_t length;
  string str_fingerprint;

  void convert(){
    str_fingerprint.clear();
    for(int i=0;i<20;i++){
      unsigned int target = fingerprint[i];
      unsigned int upper = (target&0xF);
      unsigned int lower = target>>4;
    //cout<<"target = "<< target <<" upper = "<< upper <<"  " <<to_string(upper)<<" lower = "<<lower<<"  "<<to_string(lower)<<endl;
   

      std::stringstream u_stream;
      std::stringstream l_stream;
      u_stream << std::hex << upper;
      l_stream << std::hex << lower;
      
      str_fingerprint += u_stream.str();
      str_fingerprint += l_stream.str();
    }
    assert(str_fingerprint.size() == 40);
  }

};

std::mutex foo[THREAD_NUM];
std::queue<pair<string, bool>> Q[THREAD_NUM];

std::mutex fp_lock;
std::queue<std::vector<fp_type>> fp_q;

std::atomic<int> a_end;

void initialize(){

  a_end = 0;
  sum_chunks = 0;
  sum_bytes = 0;
  sum_dup = 0;
  sample_chunks = 0;
  sample_bytes = 0;
  sample_dup = 0;

  s_fp_map.clear();
  fp_map.clear();

  h_index = 0;
  s_h_index = 0;
  
  memset(d_ratio_record, 0, sizeof(int)*101);
  memset(s_d_ratio_record, 0, sizeof(int)*101);

  sum_files = 0;
  sample_files = 0;

  memset(ref_cnt_dis, 0, sizeof(int)*10000000);
  memset(s_ref_cnt_dis, 0, sizeof(int)*10000000);
  memset(t_ref_cnt_dis, 0, sizeof(int)*10000000);
}

void print_ref_cnt(){
  int max = 0;
  string max_fp = "";
  for(auto& fp:fp_map){
    if(max<fp.second) {
      max = fp.second;
      max_fp = fp.first;
    }
    if(fp.second == 3072){
      cout<<"3072 fp = " << fp.first << endl;
    }
    ref_cnt_dis[fp.second]++;
  }


  printf("max_ref_cnt = %d\n",max);
  cout<< "max_fp = " << max_fp << endl;
  int sum=0;
  int t_chunks = 0;
  for(int i=1;i<=max;i++){
    sum += ref_cnt_dis[i]*i;
    t_chunks += ref_cnt_dis[i];
  }
  printf("sum = %d, t_chunks = %d\n", sum, t_chunks);
  printf("in whole average = %.3f\n", (double)sum/t_chunks);

  for(int i=0;i<=max;i++) printf("ref_cnt_dis[%d] = %d\n", i,ref_cnt_dis[i]);
}

void s_print_ref_cnt(){
  
  int max = 0;
  
  for(auto& s_fp:s_fp_map){
    if(max<s_fp.second) max = s_fp.second;
    s_ref_cnt_dis[s_fp.second]++;
  }

  int sum=0;
  int t_chunks =0;
  for(int i=1;i<=max;i++){
    sum += s_ref_cnt_dis[i]*i;
    t_chunks += s_ref_cnt_dis[i];
  }
  printf("s_ref_cnt_dis[0] = %d\n", s_ref_cnt_dis[0]);
  printf("sum = %d t_chunks = %d\n", sum, t_chunks);
  if(t_chunks > 0){
    printf("in sample average = %.3f\n", (double)sum/t_chunks);
  }
  else{
    printf("t_chunks = 0\n");
  }
  for(int i=0;i<=max;i++) printf("s_ref_cnt_dis[%d] = %d\n", i, s_ref_cnt_dis[i]);
}

void search_sample(){
  
  int max = 0;
  for(auto& s_fp:s_fp_map){
    auto ret = fp_map.find(s_fp.first);
    if(ret == fp_map.end()){
      printf("fp = %s\n", s_fp.first.c_str());
      assert(false);  
    }
    else{
      if(max<ret->second) max = ret->second;
      t_ref_cnt_dis[ret->second]++;
    }
  }

  printf("max = %d\n",max);
  int sum=0;
  int t_chunks = 0;
  for(int i=1;i<=max;i++){
    sum += t_ref_cnt_dis[i]*i;
    t_chunks += t_ref_cnt_dis[i];
  }
  printf("sum = %d t_chunks = %d\n", sum, t_chunks);
  if(t_chunks > 0){
    printf("sample chunks in whole average = %.3f\n", (double)sum/t_chunks);
  }
  else{
    printf("t_chunks = 0\n");
  }
  for(int i=0;i<=max;i++) printf("t_ref_cnt_dis[%d] = %d\n", i, t_ref_cnt_dis[i]);
}

int exist(string input, bool sample){
 // printf("here is exist\n");
  if(sample){
    const auto& ret = s_fp_map.find(input);
    if(ret == s_fp_map.end()) return false;
    ret->second++;
    return true;
  }
  else{
    const auto& ret = fp_map.find(input);
    if(ret == fp_map.end()) return false;
    ret->second++;
    return ret->second;
  }
}

void add_to_ht(string input, bool sample){
  
  pair<string, int> pp(input, 1);

  if(sample){
    s_fp_map.insert(pp);
  }
  else{
    auto ret = fp_map.insert(pp);
    assert(ret.second == true);
  }
}

void process_fingerprint(){
  while(true){
    size_t duplicated = 0;
    size_t bytes = 0;
    bool sample = false;

    fp_lock.lock();
    if(fp_q.empty()){
      if(a_end == THREAD_NUM+1){
        fp_lock.unlock();
        break;
      }
      fp_lock.unlock();
      continue;
    }
    auto vec_fingerprint = fp_q.front(); 
    fp_q.pop();
    fp_lock.unlock();
   
   // printf("fingerprint count = %ld\n", vec_fingerprint.size());

    for(auto &fp : vec_fingerprint){
      int exi = exist(fp.str_fingerprint, fp.sample);
      //printf("exi = %d\n", exi);
      if(exi == 0){
        add_to_ht(fp.str_fingerprint, fp.sample);
       // printf("length = %d\n",fp.length);
      } 
      else if(exi == 2){
        dup_origin += fp.length;
        //fp.length *= 2;
      }
      
      if(exi){
        duplicated += fp.length;
      }

      sample = fp.sample;
      bytes += fp.length;
    }
  
      double d_ratio=0;
      if ( bytes > 0 && duplicated > 0){
          d_ratio =((double)duplicated/(double)bytes) * 100;            
       //   printf("bytes = %lu\t duplicated = %lu\t d_ratio = %.3f\n", bytes, duplicated, (double)duplicated/(double)bytes * 100);
          
      }
      if(sample){
        
        s_d_ratio_record[(int)d_ratio]++;
        sample_chunks += vec_fingerprint.size();
        sample_bytes += bytes;
        sample_dup += duplicated;
      }
      else{
        d_ratio_record[(int)d_ratio]++;

        sum_chunks += vec_fingerprint.size();
        sum_bytes += bytes;
        sum_dup += duplicated;
      }
  }
 // printf("process_fingerprint end\n");
}


string max_file = " ";

void FP(pair<string, bool>&input_path){
      
      uint8_t buf[1024*1024];
      size_t bytes = 0;

      struct rabin_t *hash;
      hash = rabin_init();

      unsigned int chunks = 0;
      FILE* in_file;
      in_file = fopen(input_path.first.c_str(), "r");
      if(in_file == NULL) {
        printf("path = %s\n", input_path.first.c_str());
        printf("file open error  %d\n", errno);
        assert(false);
        return;
      }

      std::vector<fp_type> vec_fingerprint;
      fp_type fp;
      fp.sample = input_path.second;

      size_t gar_len = 0;

      SHA1_CTX ctx;

      SHA1Init(&ctx);

      string temp = " ";

      while (!feof(in_file)) {
          size_t len = fread(buf, 1, sizeof(buf), in_file);
          uint8_t *ptr = &buf[0];

          bytes += len;
          while (1) {
              int remaining = rabin_next_chunk(hash, ptr, len);

              if (remaining < 0) {
                  if(len > 0){
                    gar_len += len;
                    SHA1Update(&ctx, ptr, len);
                  }
                  break;
              }

              SHA1Update(&ctx, ptr, (hash->last_chunk.length-gar_len));
              gar_len = 0;
              SHA1Final(fp.fingerprint, &ctx);

              fp.length = hash->last_chunk.length; 
              fp.convert();

              
              if(fp.str_fingerprint == "06acbc3f7de2e1874302d36a8030b7b18fb3048e"){
                  
                  if(temp!=input_path.first.c_str()){
                    printf("file name = %s\n", input_path.first.c_str());
                    f_cnt++;
                    ref_cnt_file = 1;
                  }
                  else{
                    ref_cnt_file++;
                  }
                  
                  if(max_ref_cnt_file<ref_cnt_file){
                    max_ref_cnt_file = ref_cnt_file;
                    max_file = input_path.first.c_str();
                  }

                  temp = input_path.first.c_str();
                  //printf("%d %016llx\n", hash->last_chunk.length, (long long unsigned int)hash->last_chunk.cut_fingerprint);
                  for(int i = 0;i<remaining;i+=4){
                    printf("%d", *(int*)(&ptr[i]));
                  }
                  printf("\n");
              }

              
             /* if(fp.str_fingerprint == "4b5afc2a337f9f6b6644ffbb3d875f1199bf9247"){
                  printf("file name = %s\n", input_path.first.c_str());
                  
                  for(int i = 0;i<remaining;i++){
                    printf("%c", ptr[i]);
                  }
                  printf("\n");
              }
*/
              vec_fingerprint.push_back(fp);

              SHA1Init(&ctx);
              len -= remaining;
              ptr += remaining;
              
              chunks++;
          }
      }

      if (rabin_finalize(hash) != NULL) {
          chunks++;
     //     printf("%d %016llx\n", last_chunk.length,(long long unsigned int)last_chunk.cut_fingerprint);

          SHA1Final(fp.fingerprint, &ctx);
          
          fp.length = hash->last_chunk.length; 
          fp.convert(); 
          vec_fingerprint.push_back(fp);
          
          if(fp.str_fingerprint == "06acbc3f7de2e1874302d36a8030b7b18fb3048e"){
              printf("file name = %s\n", input_path.first.c_str());

            //  exit(0);
          }
      }

      fp_lock.lock();
      

//      printf("bytes = %ld \t sample no sample = %d\n", bytes, input_path.second);
//      printf("vec_fingerprint size = %ld\n", vec_fingerprint.size());
      fp_q.push(vec_fingerprint);
      

      fp_lock.unlock();


      fclose(in_file);
      free(hash);
}

void read_recursive(DIR* input_dir, char input_path[10000], int s_ratio){
    int i = 0;
    struct dirent* entry = NULL;

    while( (entry = readdir(input_dir)) != NULL) {
        char temp[10000] = {' '};
        struct stat s_buf;
        lstat(entry->d_name, &s_buf);
    
        if(entry->d_type == DT_REG){
        //  printf("FILE = %s \n", entry->d_name);
          
          strcpy(temp, input_path);
          strcat(temp, "/");
          strcat(temp, entry->d_name);
          
          if((rand()%100+1)<s_ratio+1){ //sampling
            
            foo[i].lock();
            Q[i].push({string(temp), true});
            foo[i].unlock();
            i++;
            i %= THREAD_NUM;

            sample_files++;
          }
          
          foo[i].lock();
          Q[i].push({string(temp), false});
          foo[i].unlock();
          i++;
          i %= THREAD_NUM;

       //   FP(temp);
          sum_files++;
        }
        else if(entry->d_type == DT_DIR) {
        //  printf("DIR = %s \n", entry->d_name);
          if(entry->d_name[0] != '.'){
            strcpy(temp, input_path);
            strcat(temp, "/");
            strcat(temp, entry->d_name);
         //   printf("%s\n",temp);
            DIR* dir1 = opendir(temp);
            if(dir1 == NULL){
              
              
              printf("error  %d\n", errno);

             // FP(temp);
              continue;
            }
            read_recursive(dir1, temp, s_ratio);
            closedir(dir1);
          }
        }
    }

}

void print_result(){
    printf("sum_chunks = %lld\n", sum_chunks);
    printf("sample_chunks = %lld\n", sample_chunks);
    printf("h_index = %d\n", h_index);
    printf("sum_files = %d\n", sum_files);
    printf("sample_files = %d\n", sample_files);
    printf("sample_bytes = %lld\n", sample_bytes);
    printf("sample_dup = %lld\n", sample_dup);
    printf("sample_d_ratio = %.3f\n", (double)sample_dup/(double)sample_bytes * 100);
    
    printf("sum_dup = %lld\n", sum_dup);
    printf("sum_bytes = %lld\n",sum_bytes);
    printf("sum_d_ratio = %.3f\n", ((double)sum_dup+(double)dup_origin)/(double)sum_bytes * 100);
    printf("dup_origin = %lld\n", dup_origin);
}


void polling_func(int i){
  while(true){
    foo[i].lock();
    if(Q[i].empty()){
      if(a_end >= 1){ 
        foo[i].unlock();
        break;
      }
      foo[i].unlock();
      continue;
    }
    auto cur = Q[i].front();
    Q[i].pop();
    foo[i].unlock();
    FP(cur);
  }
  a_end++;
  //printf("polling_func end\n");
}

void start_function(int sample_ratio){
    DIR* dir = opendir(src_dir);

    read_recursive(dir, src_dir, sample_ratio);

    closedir(dir);
    
    assert(a_end == 0);
    a_end = 1;
}

int main(void) {
    srand(time(NULL));

    int arr[5] = {1,5,10,20,30};
    
    for(int a = 0; a<1;a++){
      for(int k=0; k<1;k++){
        printf("start %d\n", arr[k]);
       

        initialize();
       std::vector<std::thread> threads;
     //  start_function(100);
        threads.push_back(std::thread(start_function, arr[k]));
       
        for (int i=0; i<THREAD_NUM; ++i)
          threads.push_back(std::thread(polling_func, i));

       // polling_func(0);
      //  process_fingerprint();
        threads.push_back(std::thread(process_fingerprint));

        for (auto& th : threads) th.join();


        print_result();
        print_ref_cnt();
        s_print_ref_cnt();
        search_sample();
        fflush(stdout);
      }
    }
    cout<<"f_cnt = "<<f_cnt<<endl;
    cout<<"max ref_cnt in file = "<<max_ref_cnt_file<<endl;
    cout<<"max_file = "<<max_file;
    return 0;
}
